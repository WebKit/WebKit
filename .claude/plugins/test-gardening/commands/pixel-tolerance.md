---
name: pixel-tolerance
description: Adjust fuzzy pixel tolerance in WPT/layout test files for constant IMAGE failures, handling positional, named, and SVG namespace formats
user_invocable: true
---

# Pixel Tolerance Skill

Adjusts the `<meta name="fuzzy">` tag in layout test files to fix constant IMAGE failures where
the actual pixel difference exceeds the allowed tolerance. Handles the full OpenSource workflow:
edit, Bugzilla bug, commit, and PR.

## Prerequisites

- Invoke from within a WebKit OpenSource checkout (any subdirectory). The skill derives the repo root via `git rev-parse --show-toplevel`.
- Clean working tree on `main` branch (or will switch to it)

## Supported Fuzzy Formats

The skill handles all fuzzy meta tag formats found in WebKit layout tests:

| Format | Example | Where Found |
|--------|---------|-------------|
| **Positional** | `content="0-1;0-50"` | WPT CSS/SVG tests |
| **Named** | `content="maxDifference=0-39; totalPixels=0-180000"` | WebKit-native tests |
| **SVG namespace** | `<html:meta name="fuzzy" content="..."/>` | WPT SVG reftests (.svg files) |
| **Reference-keyed** | `content="ref.html:0-1;0-50"` | Multi-reference WPT tests |

## Workflow

### Step 0: Resolve OpenSource Root

Before any other step, resolve the OpenSource repo root from the current invocation directory:

```bash
OPENSOURCE_ROOT=$(git rev-parse --show-toplevel)
```

Use this value (referred to below as `${OPENSOURCE_ROOT}`) in all subsequent file paths and `cd` targets. If `git rev-parse` fails, bail out and ask the user to re-invoke from within their OpenSource checkout.

### Step 1: Parse Arguments

Check ARGUMENTS for:
- **Radar URL**: `rdar://NNNNNNN`
- **Test path**: Layout test path (e.g., `imported/w3c/web-platform-tests/svg/shapes/reftests/pathlength-002.svg`)
- **Test run output**: The output line showing allowed vs actual fuzziness values

If test run output is provided, parse it. Example formats:

```
allowed fuzziness {'max_difference': [0, 1], 'total_pixels': [0, 50]}, actual difference {'max_difference': 1, 'total_pixels': 57}
```

Extract:
- `allowed_max_diff_range`: e.g., `[0, 1]`
- `allowed_total_pixels_range`: e.g., `[0, 50]`
- `actual_max_diff`: e.g., `1`
- `actual_total_pixels`: e.g., `57`

If not all values are provided, prompt the user via AskUserQuestion.

### Step 2: Gather Radar Info (if radar provided)

If a radar URL was provided, read the radar to extract:
- Test path
- Platform
- Failure type

### Step 3: Read Current Test File

Locate the test file at `${OPENSOURCE_ROOT}/LayoutTests/{test_path}`.

Read the file and find the existing fuzzy meta tag. The tag can appear in several forms:

**HTML files:**
```html
<meta name="fuzzy" content="0-1;0-50">
<meta name="fuzzy" content="maxDifference=0-1; totalPixels=0-50" />
<meta name=fuzzy content="0-20;0-56">
```

**SVG files (XML namespace):**
```xml
<html:meta name="fuzzy" content="0-1;0-50"/>
<html:meta name="fuzzy" content="maxDifference=0-20;totalPixels=0-29400" />
```

**Reference-keyed (positional or named):**
```html
<meta name="fuzzy" content="ref.html:0-1;0-50">
```

Parse the current tolerance values from whichever format is found. Use these regex patterns:

1. **Positional**: `content="(?:[\w.-]+:)?(\d+)-(\d+);(\d+)-(\d+)"`
2. **Named**: `content="(?:[\w.-]+:)?maxDifference=(\d+)-(\d+);\s*totalPixels=(\d+)-(\d+)"`

Show the current values to the user:
```
Current fuzzy tolerance: max_difference=0-1, total_pixels=0-50
Actual difference:       max_difference=1,   total_pixels=57
```

### Step 4: Calculate New Tolerance

For each dimension (max_difference, total_pixels):
- If actual value **exceeds** the allowed max, bump the max to the actual value **rounded up to the nearest 10**
  - Examples: 57 -> 60, 123 -> 130, 200 -> 200 (already a multiple of 10)
  - This provides a small buffer for minor platform variance
- If actual value is **within** the allowed range, keep the existing value unchanged
  - In particular: if `actual_max_diff` is within range, do NOT change `max_difference`

Show the proposed change:
```
Current:  0-1;0-50
Proposed: 0-1;0-60
```

**Ask user for approval** via AskUserQuestion before editing:
- **Question**: "The test's pixel tolerance needs adjustment. Approve the proposed values?"
- **Options**:
  1. "Apply proposed values (Recommended)" - Use the calculated values
  2. "Use exact actual values" - Set max to exact actual values without rounding
  3. "Enter custom values" - Let user specify their own values

**CRITICAL**: Do NOT edit the file until the user approves.

### Step 5: Edit the Test File

Use the Edit tool to replace the old fuzzy content with the new values.

**Preserve the original format**:
- If the original was positional (`0-1;0-50`), write positional
- If the original was named (`maxDifference=0-1; totalPixels=0-50`), write named
- If the original used `html:meta`, keep `html:meta`
- If the original was reference-keyed (`ref.html:0-1;0-50`), keep the reference prefix
- Preserve spacing (e.g., `; ` vs `;` between values)
- Preserve quote style and self-closing tag style

Example Edit for positional format in SVG:
- old_string: `content="0-1;0-50"`
- new_string: `content="0-1;0-60"`

After editing, show the diff:
```bash
cd ${OPENSOURCE_ROOT} && git diff LayoutTests/{test_path}
```

### Step 6: Create Bugzilla Bug

**Determine component** from the test path:
- `svg/` in path -> Component: "SVG"
- `css/` in path -> Component: "CSS"
- `html/` in path -> Component: "DOM"
- Other -> Component: "Layout and Rendering" (default, confirm with user)

**Bug content**:

Title: `Adjusting pixel tolerance for {test_filename}`

Description (write to `/tmp/pixel_tolerance_bug_desc_XXXXXX.txt`):
```
The test {test_path} has a constant IMAGE failure. The test's allowed
fuzziness is {old_values} but the actual difference is {actual_values}.

Bump the fuzzy pixel tolerance to accommodate the actual rendering
difference.
```

**Radar integration** (if radar provided):
- Add `InRadar` keyword
- CC `webkit-bug-importer@group.apple.com`
- Include `<rdar://RADAR_ID>` at end of description (with angle brackets - required format)

**Show the proposed bug content** to the user and ask for approval via AskUserQuestion:
- **Question**: "Review the Bugzilla bug content. How should I proceed?"
- **Options**:
  1. "Create bug (Recommended)"
  2. "Modify title or description"
  3. "Skip bug creation" - If bug already exists

**Create the bug** via `git-webkit create-bug`. Authentication is handled by webkitcorepy's keyring layer; no manual credential plumbing needed.

```bash
cd ${OPENSOURCE_ROOT} && Tools/Scripts/git-webkit create-bug \
    --title 'Adjusting pixel tolerance for {test_filename}' \
    -F /tmp/pixel_tolerance_bug_desc_XXXXXX.txt \
    --component '{COMPONENT}' \
    --project 'WebKit' \
    --keywords InRadar \
    --cc webkit-bug-importer@group.apple.com \
    --radar 'rdar://{RADAR_ID}'
```

If no radar was provided, omit `--keywords` and `--radar`. Keep `--cc webkit-bug-importer@group.apple.com` so a radar is auto-created for the bug.

Parse the command output for the new bug URL (`Created bug: https://bugs.webkit.org/show_bug.cgi?id=NNNNNN`). Clean up the description file.

### Step 7: Commit

**Write commit message** to `/tmp/pixel_tolerance_commit_XXXXXX.txt` using the Write tool:

The commit message format has a blank first line, then the bug title, then bug/radar refs,
then the review line and description. `git-webkit pr` uses line 2 as the title.

```

{Bug title}
webkit.org/b/{BUG_ID}
rdar://{RADAR_ID}

Reviewed by NOBODY (OOPS!).

Adjust fuzzy pixel tolerance for {test_filename} which has a constant
IMAGE failure. The allowed fuzziness was insufficient for the actual
rendering differences.

* LayoutTests/{test_path}:
```

**CRITICAL**: Use the Write tool for the commit message file. Never pass "(OOPS!)" through
a shell command - zsh will mangle the `!`.

**Show the commit message** and ask for approval via AskUserQuestion:
- **Question**: "Review the commit message (or edit `/tmp/pixel_tolerance_commit_XXXXXX.txt` directly). How should I proceed?"
- **Options**:
  1. "Commit (Recommended)"
  2. "Modify commit message"
  3. "Cancel"

After approval, stage and commit on `main` as separate tool calls:

```bash
cd ${OPENSOURCE_ROOT} && git add LayoutTests/{test_path}
```
```bash
cd ${OPENSOURCE_ROOT} && git commit -F /tmp/pixel_tolerance_commit_XXXXXX.txt
```
```bash
rm /tmp/pixel_tolerance_commit_XXXXXX.txt
```

### Step 8: Push PR

`git-webkit pr` reads the bug/radar info from the commit message, creates the branch automatically, and posts the PR.

Ask for confirmation before pushing:
- **Question**: "Ready to push the PR?"
- **Options**:
  1. "Push PR (Recommended)"
  2. "Not yet"

```bash
cd ${OPENSOURCE_ROOT} && Tools/Scripts/git-webkit pr
```

Report the PR URL from the output.

## Error Handling

- **Test file not found**: Check path, suggest alternatives with Glob
- **No fuzzy meta tag found**: Add one inline via Edit. For WPT tests use positional format (`<meta name="fuzzy" content="0-N;0-M">`); for WebKit-native tests use the named format (`<meta name="fuzzy" content="maxDifference=0-N; totalPixels=0-M">`). Determine initial values from the test run output.
- **Radar fetch fails**: Prompt user for title and test path manually
- **Bug creation fails**: Show error, offer to retry or skip
- **Dirty working tree**: Alert user to stash or commit existing changes first
- **git-webkit branch fails**: May already be on a branch; show status and ask user

## Examples

### Example 1: SVG test with positional format

```
User: /pixel-tolerance rdar://172914523
Claude: [Fetches radar: "imported/w3c/.../pathlength-002.svg is a constant IMAGE failure"]
Claude: [Reads test file, finds: <html:meta name="fuzzy" content="0-1;0-50"/>]
Claude: [Parses test output: actual max_difference=1, actual total_pixels=57]
Claude: Current: 0-1;0-50 -> Proposed: 0-1;0-60
Claude: [Asks for approval]
User: Apply proposed values
Claude: [Edits file, shows diff]
Claude: [Creates Bugzilla bug: "Adjusting pixel tolerance for pathlength-002.svg"]
Claude: [Creates branch, commits, pushes PR]
```

### Example 2: CSS test with named format

```
User: /pixel-tolerance
       Test: css3/masking/mask-repeat-space-border.html
       Actual: max_difference=5, total_pixels=120000
Claude: [Reads test, finds: maxDifference=0-2;totalPixels=30-106000]
Claude: Current: maxDifference=0-2;totalPixels=30-106000
         Proposed: maxDifference=0-5;totalPixels=30-120000
Claude: [Asks for approval, edits, creates bug, commits, pushes]
```

### Example 3: Only total_pixels needs bumping

```
User: /pixel-tolerance
       Test: imported/w3c/.../pathlength-002.svg
       Allowed: max_difference=[0,1], total_pixels=[0,50]
       Actual: max_difference=1, total_pixels=57
Claude: max_difference (1) is within allowed range [0,1] - no change needed
         total_pixels (57) exceeds allowed max (50) - bumping to 60
Claude: Current: 0-1;0-50 -> Proposed: 0-1;0-60
```
