---
name: webrtc-changelog
description: Generate a professional HTML changelog from WebRTC git logs. Automatically categorizes changes, includes a high-level summary, and provides detailed commit and review links.
---

# WebRTC Changelog Generator

`webrtc-changelog` automates the generation of a professional, categorized HTML
report for WebRTC changes.

## Workflow

1. **Fetch Branch Information:** Navigate to
   `https://chromiumdash.appspot.com/branches` and extract the current and
   previous branch numbers associated with the milestone from the **webrtc**
   column of the first two rows in the **Branch details** table. These will be
   used as `<branch2>` (current) and `<branch1>` (previous). Note the milestone
   number (e.g., 148).
2. **Update Git** Execute `git fetch origin` to retrieve the remote git
   branches.
3. **Fetch Git Log:** Execute
   `git log branch-heads/<branch1>..branch-heads/<branch2>` to retrieve the
   commit history.
4. **Generate Summary:** Analyze the retrieved git log and produce a high-level
   summary of the most significant changes. The summary MUST be formatted as an
   HTML unordered list (`<ul><li>...</li></ul>`) for best presentation in the
   report.
5. **Generate HTML Report:** Use the `generate_changelog.py` script to create
   the final report, injecting the generated HTML summary and the milestone:
   ```bash
   python3 tools_webrtc/generate_changelog.py <branch1> <branch2> <output_file> --summary_text "<summary_html>" --milestone M<milestone_number>
   ```

## Features

### AI-Powered Summary

- High-level, one-pager overview of the release.
- Automatically includes commit and author statistics.

### Categorized Changes

- Automatically categorizes commits based on keywords in the commit.
- Provides 📝 (Commit) and 🔍 (Review/Gerrit) links for each entry.
- Automatically converts `Bug:` references (e.g., `webrtc:123`, `chromium:456`,
  `b/789`) into hyperlinks.
