---
name: cl-description
description: Use this skill to draft, write, review, or format a WebRTC Change List (CL) description or Git commit message following WebRTC's guidelines and presubmit rules. Trigger this whenever the user needs help documenting code changes for review or updating the description of an existing Gerrit CL, even if they don't explicitly mention "CL". Do not use this skill for formatting code, explaining logic, or performing general code reviews.
---

# CL Description Skill

This skill guides generating, refining, and formatting high-quality Change List
(CL) descriptions and Git commit messages for WebRTC contributions.

A CL description is the permanent historical record of a change. It is read by
reviewers today and by engineers bisecting regressions years from now. It is
also parsed by infrastructure (presubmit checks, Gerrit, the issue tracker and
the release notes tooling). A good description clearly explains the intent
("why") and the approach ("what"), and has well-formed footers.

## 1. Pre-flight Investigation (Interactivity)

Before generating the final draft, analyze the diff and the session history. If
any of the following are missing or ambiguous, **STOP and ask the user for
clarification** instead of guessing:

- **The "Why":** The technical rationale or motivation is not clear from the
  diff, the session history, or the linked bug.
- **Bug ID:** No bug was mentioned. Ask whether one should be associated and
  suggest a likely candidate from local history (see below) when available. If
  the user confirms there is none, use `Bug: none`.
- **Bug Tracker:** A bare bug number was given without a tracker. Confirm
  whether it is a WebRTC (`webrtc:`), Chromium (`chromium:`), or internal
  Buganizer (`b/`) issue.
- **Bug vs. Fixed:** It is unclear whether landing the CL fully resolves the bug
  (`Fixed:`) or is only part of the work (`Bug:`).
- **Subject Prefix:** Nearby history consistently uses a component prefix (see
  below) but it is unclear which one applies to this change.

### Using Local History

The Git history of the touched files is the best source of local conventions.
Use it to suggest, never to decide, values for the subject prefix and bug:

```bash
git log -n 20 --format='%s%n%b' @{u} -- <paths touched by the CL>
```

- Treat matches as suggestions. If entries conflict or the match is uncertain,
  ask the user.
- Only reuse a bug from history if the current change is clearly part of the
  same work item. Do not attach a general feature bug to an unrelated regression
  fix or crash fix.

## 2. Formatting Constraints (Mandatory)

- **72-Column Wrap:** Every line of the subject and body **MUST** be
  hard-wrapped at 72 characters. Exceptions: URLs, footers and quoted material
  such as log lines or code must not be broken.
- **Subject Line:**
  - A single line in the imperative mood (e.g., "Add ...", "Fix ...", "Remove
    ..."). Not past tense ("Added") or third person ("Adds").
  - Aim for 50 characters or fewer; 72 characters is the hard limit.
  - Start with a capital letter and do not end with a period.
  - Accurately summarize the primary change.
- **Optional Subject Prefix:** WebRTC does not require a component prefix. Use
  one only when the touched area conventionally uses it, e.g. `[dcsctp] ...`,
  `pc: ...`, `sdk: ...`, `dav1d: ...`. When used, the prefix counts toward the
  subject length.
- **Subject Spacing:** There **MUST** be exactly one blank line between the
  subject and the body.
- **Paragraphs:** Separate paragraphs with a single blank line. Use `-` bullet
  points for multi-part changes, indenting continuation lines to align with the
  bullet text.
- **No Markdown Hyperlinks:** Do not use markdown-style links (e.g.,
  `[text](url)`), headings, or emphasis markup. Gerrit renders plain text.
- **Footer Block:** Precede the footer block with exactly one blank line and do
  not put blank lines between footers. Git and Gerrit trailer parsers require
  footers to form a single contiguous block at the end of the message.

## 3. Body Content Requirements

- **Why over What:** Do not merely list changed files or functions; the diff
  already shows that. Explain why the change is necessary.
- **Before and After:** Describe the baseline behavior or problem ("before"),
  then the solution and new behavior ("after"). Call out design choices,
  trade-offs, and non-obvious details such as threading, ownership, or API
  compatibility implications.
- **Paraphrase Bugs:** Summarize the underlying problem in your own words rather
  than copy-pasting raw issue tracker text.
- **Short Changes:** The body may be omitted if the diff is small and the
  subject is fully self-explanatory (e.g., a typo fix).
- **Omit Noise:** Do not include routine test execution details (e.g., "ran
  rtc_unittests"), execution logs, developer chatter, or hypothetical musings.
  Automated test coverage is visible in the diff and on the CQ.
- **Links:** Only include links (design docs, other CLs, external specs) when
  they add essential context or are requested. Use full URLs, e.g.
  `https://webrtc-review.googlesource.com/c/src/+/NUMBER` for WebRTC CLs and
  `https://crrev.com/c/NUMBER` for Chromium CLs. Never link to internal-only
  resources from a public CL.
- **API Changes:** If the CL changes, deprecates, or removes a public API (under
  `api/` or `sdk/`), state this explicitly and describe the migration path for
  downstream users.

### Conciseness

Verbose descriptions bury the important information and are skimmed rather than
read. Prefer the shortest description that conveys the rationale.

- **Scale to the Change:** Match the length of the body to the size and risk of
  the change. A small or mechanical CL needs one or two sentences, or no body at
  all.
- **Soft Cap:** Aim for a body of at most a few short paragraphs. If more is
  needed, the rationale likely belongs in the bug or a design doc; summarize and
  link to it instead.
- **No Repetition:** Do not restate the subject in the first sentence of the
  body, and do not narrate the diff line by line.
- **Few, High-Level Bullets:** Use a handful of bullets at most, each describing
  a logical change rather than an individual function or file.
- **No Filler:** Avoid preambles such as "This CL ..." or "In this change we
  ...", closing summaries, and restating benefits that are obvious from the
  rationale.

## 4. Critical Footer Logic

WebRTC's `PRESUBMIT.py` (`CheckChangeHasBugField` and
`CheckCommitMessageBugEntry`) enforces the following, so these rules are not
optional:

- **Bug Footer (Mandatory):** Every CL must have a `Bug:` (or `Fixed:`) footer.
- **Tracker Prefix Required:** Bare numbers are rejected. Use:
  - WebRTC issues: `Bug: webrtc:12345`
  - Chromium issues: `Bug: chromium:123456`
  - Internal Buganizer issues: `Bug: b/123456789`
- **No Bug:** If no bug is associated, use `Bug: none`. Unlike Chromium, do
  **not** omit the footer.
- **Multiple Bugs:** Comma-separate them on one line, e.g.
  `Bug: webrtc:12345, chromium:123456`.
- **Closing Bugs:** Use `Fixed:` instead of `Bug:` when landing the CL should
  close the bug automatically, e.g. `Fixed: webrtc:12345`.
- **Manual Verification (Optional):** If the change was verified manually in a
  way not covered by automated tests (e.g., in Chrome or a demo app), a `Test:`
  footer may briefly describe how. Do not use it to list unit test runs.
- **Change-Id:** Preserve an existing `Change-Id:` footer verbatim when amending
  or updating an existing CL. It must remain the last footer in the block. Never
  invent one; the Git commit-msg hook generates it.
- **Other Footers:** Preserve other existing footers (e.g., `No-Try:`,
  `Reviewed-on:`, `Original-Commit-Position:`) unless the user asks otherwise.
- **Single Line:** Each footer must stay on a single line, even if it exceeds 72
  characters.

### Reverts and Relands

- Keep the subject generated by Gerrit or `git revert`, e.g.
  `Revert "Original subject"` or `Reland "Original subject"`.
- For a reland, keep the quoted original description and add a short paragraph
  at the top explaining what changed since the revert and why it is now safe to
  land (e.g., "Patchset 1 is the original CL. Patchset 2 fixes the data race
  reported by TSan.").

## 5. Step-by-Step Workflow

1. **Analyze the Patch:** Examine the diff against the configured upstream
   branch (e.g., `git diff @{u}`), not necessarily `origin/main`. Identify key
   modifications, affected subsystems, threading and ownership implications, and
   overall intent.
2. **Review Context and Existing Description:** Read any existing commit message
   (`git log -1 --format=%B`) and the linked bug. If the CL has already been
   uploaded, check its description on Gerrit (`git cl description -d`). Note
   missing rationale or poor formatting.
3. **Run Pre-flight Investigation:** Resolve open questions from Section 1 with
   the user before drafting.
4. **Draft:** Write the subject, body, and footers following Sections 2-4.
5. **Wrap Mechanically:** LLMs cannot reliably count columns. Save the draft to
   a scratch file outside the repository (e.g., under `out/<dir>/`) and run it
   through the provided wrapping script:
   ```bash
   vpython3 agents/skills/cl-description/scripts/wrap_lines.py draft.txt
   ```
   The script wraps the subject-separated body at 72 columns, preserves bullet
   indentation, and leaves footers, URLs and indented blocks untouched.
6. **Verify Compliance:** Check the subject length and mood, the blank line
   after the subject, body wrapping, the contiguous footer block, a well-formed
   `Bug:` footer, and a preserved `Change-Id:`. Re-read the body and cut any
   sentence that does not help a reviewer or future reader understand why the
   change was made (see Conciseness).
7. **Apply:** Present the final message in a code block. Only apply it when the
   user asks to:
   - **Local commit:** `git commit --amend -F draft.txt`.
   - **Initial upload:** `git cl upload --commit-description=+` uses the local
     commit message (see the `git-cl` skill).
   - **Existing CL:** Never overwrite an existing Gerrit description without
     explicit user intent. When asked to, run `git cl description -n +` to set
     the Gerrit description from the latest local commit message, keeping the
     existing `Change-Id:`.

## Example

```text
Simplify frame drop handling in VideoStreamEncoder

VideoStreamEncoder currently hops to the worker thread when notifying
registered frame drop observers. Because observer registration and media
pipeline reconfiguration now run directly on the encoder task queue,
hopping threads introduces unnecessary latency and complexity.

- Eliminate the TaskQueue hop in VideoStreamEncoder::OnFrameDropped.
- Guard the observer collection with the encoder queue sequence checker.
- Remove the now obsolete PendingTaskSafetyFlag from encoder state.

Bug: webrtc:12345
Change-Id: I0123456789abcdef0123456789abcdef01234567
```

## Template

```text
[Optional prefix: ]<Imperative summary, ideally <= 50 chars>

<Why: the problem, motivation, or previous behavior.>

<What: the approach, new behavior, design choices and trade-offs.
Wrap strictly at 72 columns. Omit the body only if the change is
trivially self-explanatory.>

Bug: <webrtc:ID | chromium:ID | b/ID | none>
Test: <Optional: manual verification steps>
Change-Id: <Preserved from the existing commit, if any>
```
