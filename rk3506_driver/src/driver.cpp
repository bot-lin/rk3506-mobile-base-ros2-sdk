#include <rclcpp/rclcpp.hpp>
#include <rcl_interfaces/msg/parameter_descriptor.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <std_msgs/msg/u_int8.hpp>
#include <sensor_msgs/msg/battery_state.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <tf2_ros/transform_broadcaster.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using Bytes = std::vector<uint8_t>;
using Velocity = std::array<double, 3>;
enum BaseFlag : uint16_t {
    Simulated = 1, CanTxDisabled = 2, MotorEnabled = 4, MotionFeedbackValid = 16,
    DigitalInputsValid = 32, FaultLatched = 128, OdometryValid = 256, SoftwareStop = 1024
};
uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) << 8 | p[1]; }
uint32_t u32(const uint8_t* p) { return uint32_t(u16(p)) << 16 | u16(p + 2); }
double i16(const uint8_t* p) { const auto v = u16(p); return (int32_t(v) - (v > 32767 ? 65536 : 0)) / 1000.; }
double i32(const uint8_t* p) { const auto v = u32(p); return (int64_t(v) - (v > 2147483647U ? 4294967296LL : 0)) / 1000.; }
void put32(Bytes& b, uint32_t v) { for (int i = 3; i >= 0; --i) b.push_back(v >> (i * 8)); }
double age(Clock::time_point t) { return t == Clock::time_point{} ? INFINITY : std::chrono::duration<double>(Clock::now() - t).count(); }
uint32_t session() { uint32_t value = 0; std::random_device random; while (!value) value = random(); return value; }
struct Endpoint {
    int fd{-1};
    Clock::time_point received{}, next_subscription{};
    bool valid{false};
    unsigned health{0}, malformed{0};
    ~Endpoint() { if (fd >= 0) close(fd); }
};

class Driver : public rclcpp::Node {
public:
    Driver() : Node("rk3506_driver") {
        robot_ip_ = setting<std::string>("robot_ip", "192.168.50.1");
        const std::array<int64_t, 3> ports{{setting<int64_t>("base_port", 5003),
            setting<int64_t>("battery_port", 5001), setting<int64_t>("di_port", 5002)}};
        command_timeout_ = setting<double>("command_timeout", 0.3);
        base_timeout_ = setting<double>("base_timeout", 0.5);
        battery_timeout_ = setting<double>("battery_timeout", 6.0);
        di_timeout_ = setting<double>("di_timeout", 1.0);
        max_linear_ = setting<double>("max_linear_velocity", 1.0);
        max_angular_ = setting<double>("max_angular_velocity", 1.0);
        odom_frame_ = setting<std::string>("odom_frame", "odom");
        base_frame_ = setting<std::string>("base_frame", "base_link");
        const bool stamped = setting<bool>("use_stamped_cmd_vel", false);
        const bool publish_tf = setting<bool>("publish_tf", false);
        pose_covariance_ = setting<std::vector<double>>("pose_covariance", {0.1, 0.1, 1e6, 1e6, 1e6, 0.2});
        twist_covariance_ = setting<std::vector<double>>("twist_covariance", {0.1, 0.1, 1e6, 1e6, 1e6, 0.2});
        for (const auto* covariance : {&pose_covariance_, &twist_covariance_}) {
            if (covariance->size() != 6) throw std::invalid_argument("covariance requires six diagonal entries");
            for (double value : *covariance)
                if (!std::isfinite(value) || value < 0) throw std::invalid_argument("covariance must be finite and nonnegative");
        }
        if (odom_frame_.empty() || base_frame_.empty() || odom_frame_ == base_frame_ ||
            odom_frame_.front() == '/' || base_frame_.front() == '/')
            throw std::invalid_argument("odom/base frames must be distinct nonempty relative frame IDs");
        for (double value : {command_timeout_, base_timeout_, battery_timeout_, di_timeout_, max_linear_, max_angular_})
            if (!std::isfinite(value) || value <= 0) throw std::invalid_argument("timeouts and limits must be finite and positive");
        if (max_linear_ > 32.767 || max_angular_ > 32.767)
            throw std::invalid_argument("velocity limit or port exceeds wire range");
        sockaddr_in peer{};
        peer.sin_family = AF_INET;
        if (inet_pton(AF_INET, robot_ip_.c_str(), &peer.sin_addr) != 1 || peer.sin_addr.s_addr == 0)
            throw std::invalid_argument("robot_ip must be a nonzero IPv4 address");
        if (ports[0] == ports[1] || ports[0] == ports[2] || ports[1] == ports[2])
            throw std::invalid_argument("UDP ports must be distinct");
        for (size_t i = 0; i < endpoints_.size(); ++i) {
            if (ports[i] <= 0 || ports[i] > 65535) throw std::invalid_argument("invalid UDP port");
            peer.sin_port = htons(ports[i]);
            auto& fd = endpoints_[i].fd;
            fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
            if (fd < 0 || connect(fd, reinterpret_cast<sockaddr*>(&peer), sizeof(peer)))
                throw std::runtime_error("cannot connect controller UDP socket");
        }
        odom_publisher_ = create_publisher<nav_msgs::msg::Odometry>("wheel/odometry", 10);
        battery_publisher_ = create_publisher<sensor_msgs::msg::BatteryState>("battery_state", 10);
        di_publisher_ = create_publisher<std_msgs::msg::UInt8>("digital_inputs", 10);
        diagnostic_publisher_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>("diagnostics", 10);
        if (publish_tf) tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
        if (stamped) stamped_subscription_ = create_subscription<geometry_msgs::msg::TwistStamped>("cmd_vel", rclcpp::QoS(1),
            [this](geometry_msgs::msg::TwistStamped::ConstSharedPtr msg) {
                if ((!msg->header.frame_id.empty() && msg->header.frame_id != base_frame_) ||
                    msg->header.stamp.sec < 0 || msg->header.stamp.nanosec >= 1000000000U ||
                    (msg->header.stamp.sec == 0 && msg->header.stamp.nanosec == 0)) { dropCommand(); return; }
                const double elapsed = (now() - rclcpp::Time(msg->header.stamp)).seconds();
                if (std::abs(elapsed) > command_timeout_) { dropCommand(); return; }
                command(msg->twist);
            });
        else command_subscription_ = create_subscription<geometry_msgs::msg::Twist>("cmd_vel", rclcpp::QoS(1),
            [this](geometry_msgs::msg::Twist::ConstSharedPtr msg) { command(*msg); });
        stop_service_ = create_service<std_srvs::srv::Trigger>("stop",
            [this](const std_srvs::srv::Trigger::Request::SharedPtr,
                   std_srvs::srv::Trigger::Response::SharedPtr response) { stop(*response); });
        timer_ = create_wall_timer(std::chrono::milliseconds(10), [this] { tick(); });
    }
    ~Driver() override {
        if (claimed_) control(0x11, Velocity{});
        for (size_t i = 0; i < endpoints_.size(); ++i) send({1, 2}, i);
    }
private:
    template<typename T>
    T setting(const std::string& name, const T& value) {
        rcl_interfaces::msg::ParameterDescriptor descriptor;
        descriptor.read_only = true;
        return declare_parameter<T>(name, value, descriptor);
    }
    std::array<Endpoint, 3> endpoints_;
    uint32_t session_{session()}, sequence_{0}, owner_{0};
    uint16_t flags_{0};
    bool claimed_{false}, connected_{false};
    double command_timeout_, base_timeout_, battery_timeout_, di_timeout_, max_linear_, max_angular_;
    std::string robot_ip_, odom_frame_, base_frame_;
    Velocity velocity_{};
    std::vector<double> pose_covariance_, twist_covariance_;
    Clock::time_point command_time_{}, next_control_{}, next_claim_{}, next_diagnostics_{};
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_publisher_;
    rclcpp::Publisher<sensor_msgs::msg::BatteryState>::SharedPtr battery_publisher_;
    rclcpp::Publisher<std_msgs::msg::UInt8>::SharedPtr di_publisher_;
    rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostic_publisher_;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr command_subscription_;
    rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr stamped_subscription_;
    std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr stop_service_;
    std::optional<uint8_t> stop_result_;
    uint32_t stop_sequence_{0};

    bool send(const Bytes& data, size_t channel = 0) {
        return ::send(endpoints_[channel].fd, data.data(), data.size(), MSG_DONTWAIT) == ssize_t(data.size());
    }
    void dropCommand() { velocity_ = {}; command_time_ = {}; }
    void resetControl() { dropCommand(); claimed_ = false; session_ = session(); sequence_ = 0; }
    bool fresh() const { return endpoints_[0].valid && age(endpoints_[0].received) <= base_timeout_; }
    bool ready() const {
        return fresh() && (flags_ & MotionFeedbackValid) && !(flags_ & (FaultLatched | SoftwareStop)) &&
            ((flags_ & Simulated) || ((flags_ & MotorEnabled) && (flags_ & DigitalInputsValid) && !(flags_ & CanTxDisabled)));
    }
    void command(const geometry_msgs::msg::Twist& msg) {
        const Velocity requested{{msg.linear.x, msg.linear.y, msg.angular.z}};
        if (!ready() || (owner_ && owner_ != session_)) { dropCommand(); return; }
        if (!std::isfinite(msg.linear.z) || !std::isfinite(msg.angular.x) || !std::isfinite(msg.angular.y) ||
            msg.linear.z != 0 || msg.angular.x != 0 || msg.angular.y != 0) { dropCommand(); return; }
        for (size_t i = 0; i < requested.size(); ++i)
            if (!std::isfinite(requested[i]) || std::abs(requested[i]) > (i == 2 ? max_angular_ : max_linear_)) {
                dropCommand(); return;
            }
        velocity_ = requested; command_time_ = Clock::now();
    }
    uint32_t control(uint8_t op, const Velocity& value = {}) {
        if (sequence_ == UINT32_MAX) { resetControl(); return 0; }
        Bytes bytes{1, op}; put32(bytes, session_);
        const uint32_t seq = op == 0x10 ? 0 : ++sequence_;
        if (op != 0x10) put32(bytes, seq);
        if (op == 0x11) for (auto v : value) {
            const auto scaled = static_cast<uint16_t>(static_cast<int16_t>(std::lround(v * 1000)));
            bytes.push_back(scaled >> 8); bytes.push_back(scaled & 255);
        }
        if (!send(bytes)) resetControl();
        return seq;
    }
    void receive() {
        for (size_t channel = 0; channel < endpoints_.size(); ++channel) {
        auto& endpoint = endpoints_[channel];
        for (unsigned count = 0; count < 64; ++count) {
            uint8_t packet[256];
            const auto length = recv(endpoint.fd, packet, sizeof(packet), MSG_DONTWAIT);
            if (length < 0) break;
            if (channel != 0) { sensor(channel, packet, length); continue; }
            if (length == 11 && packet[0] == 1 && packet[1] >= 0x90 && packet[1] <= 0x98) {
                if (u32(packet + 2) != session_) continue;
                if (packet[1] == 0x95 && u32(packet + 6) == stop_sequence_) stop_result_ = packet[10];
                if (packet[1] == 0x90) {
                    claimed_ = packet[10] == 0;
                    if (!claimed_) dropCommand();
                } else if (packet[10]) { dropCommand(); if (packet[10] == 1 || packet[10] == 2) resetControl(); }
                continue;
            }
            if (length != 49 || packet[0] != 1 || packet[1] != 3 || packet[8] != 0) {
                endpoint.valid = false; ++endpoint.malformed; dropCommand(); continue;
            }
            endpoint.received = Clock::now(); endpoint.valid = true;
            const auto previous_owner = owner_;
            flags_ = u16(packet + 9); owner_ = u32(packet + 41);
            if ((claimed_ && owner_ != session_) ||
                (owner_ && owner_ != session_ && previous_owner == session_)) resetControl();
            if (owner_ && owner_ != session_) {
                dropCommand(); claimed_ = false;
            }
            if (!ready()) dropCommand();
            if (!(flags_ & OdometryValid)) continue;
            nav_msgs::msg::Odometry odom;
            odom.header.stamp = now(); odom.header.frame_id = odom_frame_; odom.child_frame_id = base_frame_;
            odom.pose.pose.position.x = i32(packet + 31); odom.pose.pose.position.y = i32(packet + 35);
            const double yaw = i16(packet + 39);
            odom.pose.pose.orientation.z = std::sin(yaw / 2); odom.pose.pose.orientation.w = std::cos(yaw / 2);
            odom.twist.twist.linear.x = i16(packet + 17); odom.twist.twist.linear.y = i16(packet + 19);
            odom.twist.twist.angular.z = i16(packet + 21);
            for (size_t i = 0; i < 6; ++i) {
                odom.pose.covariance[i * 7] = pose_covariance_[i]; odom.twist.covariance[i * 7] = twist_covariance_[i];
            }
            odom_publisher_->publish(odom);
            if (tf_broadcaster_) {
                geometry_msgs::msg::TransformStamped transform;
                transform.header = odom.header; transform.child_frame_id = base_frame_;
                transform.transform.translation.x = odom.pose.pose.position.x;
                transform.transform.translation.y = odom.pose.pose.position.y;
                transform.transform.rotation = odom.pose.pose.orientation;
                tf_broadcaster_->sendTransform(transform);
            }
        }
        }
    }
    void sensor(size_t channel, const uint8_t* packet, ssize_t length) {
        auto& endpoint = endpoints_[channel];
        endpoint.received = Clock::now(); endpoint.valid = false;
        if (length < 1 || packet[0] > 3 ||
            (channel == 2 && length != 2) ||
            (channel == 1 && (packet[0] ? length != 1 : length < 17 || packet[16] > 32 || length != 17 + 2 * packet[16]))) {
            ++endpoint.malformed; endpoint.health = 4; return;
        }
        endpoint.health = packet[0];
        if (endpoint.health) return;
        if (channel == 2) {
            endpoint.valid = true;
            std_msgs::msg::UInt8 message; message.data = packet[1]; di_publisher_->publish(message); return;
        }
        if (packet[13] > 100 || packet[15] == 0) { ++endpoint.malformed; endpoint.health = 4; return; }
        sensor_msgs::msg::BatteryState message;
        const float unknown = std::numeric_limits<float>::quiet_NaN();
        message.header.stamp = now(); message.header.frame_id = base_frame_;
        message.voltage = u16(packet + 1) / 100.f;
        message.current = i16(packet + 3) * 10.f;
        message.charge = u16(packet + 5) / 100.f;
        message.capacity = u16(packet + 7) ? u16(packet + 7) / 100.f : unknown;
        message.design_capacity = unknown; message.percentage = packet[13] / 100.f;
        message.temperature = unknown;
        for (unsigned i = 0; i < packet[16]; ++i) message.cell_temperature.push_back((int(u16(packet + 17 + 2 * i)) - 2731) / 10.f);
        if (!message.cell_temperature.empty()) {
            message.temperature = 0;
            for (float value : message.cell_temperature) message.temperature += value / message.cell_temperature.size();
        }
        // NTC sensors are pack sensors, not per-cell measurements.
        message.cell_temperature.clear();
        message.power_supply_status = message.current > 0 ? message.POWER_SUPPLY_STATUS_CHARGING :
            message.current < 0 ? message.POWER_SUPPLY_STATUS_DISCHARGING : message.POWER_SUPPLY_STATUS_NOT_CHARGING;
        message.power_supply_health = u16(packet + 11) ? message.POWER_SUPPLY_HEALTH_UNSPEC_FAILURE : message.POWER_SUPPLY_HEALTH_GOOD;
        message.power_supply_technology = message.POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
        message.present = true;
        message.cell_voltage.assign(packet[15], unknown);
        endpoint.valid = true; battery_publisher_->publish(message);
    }
    void diagnostics() {
        diagnostic_msgs::msg::DiagnosticArray message;
        message.header.stamp = now();
        const std::array<std::string, 3> names{{"controller", "battery", "digital_inputs"}};
        const std::array<double, 3> timeouts{{base_timeout_, battery_timeout_, di_timeout_}};
        for (size_t i = 0; i < endpoints_.size(); ++i) {
            const auto& endpoint = endpoints_[i];
            diagnostic_msgs::msg::DiagnosticStatus status;
            status.name = std::string(get_fully_qualified_name()) + '/' + names[i]; status.hardware_id = robot_ip_;
            if (age(endpoint.received) > timeouts[i]) { status.level = status.STALE; status.message = "No recent UDP sample"; }
            else if (!endpoint.valid) { status.level = status.ERROR; status.message = "Invalid or failed sample"; }
            else if (i == 0 && (flags_ & (FaultLatched | SoftwareStop))) { status.level = status.ERROR; status.message = "Controller fault or software stop latched"; }
            else if (i == 0 && (!ready() || (owner_ && owner_ != session_) || (flags_ & Simulated))) {
                status.level = status.WARN; status.message = (flags_ & Simulated) ? "Simulated controller" : "Motion inhibited or another owner active";
            } else { status.level = status.OK; status.message = "Valid UDP sample; receive age is not measurement age"; }
            const auto add = [&](const std::string& key, const std::string& value) {
                diagnostic_msgs::msg::KeyValue field; field.key = key; field.value = value; status.values.push_back(field);
            };
            add("receive_age_seconds", std::to_string(age(endpoint.received)));
            add("sensor_health", std::to_string(endpoint.health));
            add("malformed_packets", std::to_string(endpoint.malformed));
            if (i == 0) { add("flags", std::to_string(flags_)); add("owner_session", std::to_string(owner_)); add("command_age_seconds", std::to_string(age(command_time_))); }
            message.status.push_back(status);
        }
        diagnostic_publisher_->publish(message);
    }
    void stop(std_srvs::srv::Trigger::Response& response) {
        dropCommand();
        response.success = false;
        if (!fresh() || (owner_ && owner_ != session_)) {
            response.message = "Controller unavailable or another client owns control"; return;
        }
        // The single executor pauses command transmission during this bounded ACK wait.
        const auto deadline = Clock::now() + std::chrono::milliseconds(250);
        if (!claimed_) control(0x10);
        while (!claimed_ && Clock::now() < deadline) { receive(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        if (!claimed_) { response.message = "Controller did not grant a stop session"; return; }
        control(0x11, Velocity{});
        stop_result_.reset(); stop_sequence_ = control(0x15);
        while (!stop_result_ && Clock::now() < deadline) { receive(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        if (!stop_result_) response.message = "Controller stop acknowledgement timed out";
        else if (*stop_result_) response.message = "Controller rejected stop: " + std::to_string(*stop_result_);
        else { response.success = true; response.message = "Controller acknowledged software stop; clear explicitly in web UI"; }
    }
    void tick() {
        const auto current = Clock::now();
        if (!fresh() && connected_) {
            resetControl(); connected_ = false;
            for (auto& endpoint : endpoints_) endpoint.next_subscription = current;
        }
        receive();
        connected_ = fresh();
        for (size_t i = 0; i < endpoints_.size(); ++i) {
            auto& endpoint = endpoints_[i];
            if (current >= endpoint.next_subscription) {
                send({1, 1}, i);
                endpoint.next_subscription = current + std::chrono::seconds(i == 0 && !fresh() ? 1 : 10);
            }
        }
        if (current >= next_diagnostics_) { diagnostics(); next_diagnostics_ = current + std::chrono::milliseconds(500); }
        if (current < next_control_) return;
        next_control_ = current + std::chrono::milliseconds(20);
        if (!ready()) { dropCommand(); return; }
        if (age(command_time_) > command_timeout_) dropCommand();
        if (!claimed_) {
            if (command_time_ != Clock::time_point{} && (!owner_ || owner_ == session_) && current >= next_claim_) {
                control(0x10); next_claim_ = current + std::chrono::milliseconds(250);
            }
            return;
        }
        if (owner_ == session_) control(0x11, velocity_);
    }
};
} // namespace

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    try { rclcpp::spin(std::make_shared<Driver>()); }
    catch (const std::exception& error) {
        RCLCPP_FATAL(rclcpp::get_logger("rk3506_driver"), "%s", error.what());
        rclcpp::shutdown(); return 1;
    }
    rclcpp::shutdown(); return 0;
}
