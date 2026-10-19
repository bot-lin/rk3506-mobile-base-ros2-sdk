# Issue tracker: GitHub

Issues and specs live in bot-lin/rk3506-mobile-base-ros2-sdk.
Use the gh CLI from this repository.

- Publish: gh issue create --title "..." --body-file <file>
- Read: gh issue view <number> --comments
- List: gh issue list --state open
- Comment: gh issue comment <number> --body-file <file>
- Label: gh issue edit <number> --add-label "<label>"
- Close: gh issue close <number>

Publishing a spec means creating an issue labelled ready-for-agent.
Read label mappings from triage-labels.md.

PRs as a request surface: no.
