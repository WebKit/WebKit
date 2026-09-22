---
name: xcresult
description: Use to analyze build failures using an .xcresult bundle from Xcode. Run after the build-webkit:xcode skill completes incidating a failure.
user-invocable: true
allowed-tools: Bash(${CLAUDE_SKILL_DIR}/scripts/errors.py *) Bash(${CLAUDE_SKILL_DIR}/scripts/failed-commands.py *)
---

Invoked with a path to an .xcresult bundle, presumably produced by the
`build-webkit:xcode` skill. Your task is to analyze the xcresult and report
what failed using the provided scripts.

## Read what failed

```sh
${CLAUDE_SKILL_DIR}/scripts/errors.py <xcresult>
```

Reports the build's status, how many errors each file has, and the most repeated messages with the
places they came from. Read those counts before opening any source file: a batch of errors is
usually one root cause, and one bad declaration repeats the same message at every call site. When
the report ends in `next_offset`, passing `--offset` that number reads the next page of messages.

Some errors are not semantic and contain no source location. Their cause can by
found by examing raw commands with the following command:

```sh
${CLAUDE_SKILL_DIR}/scripts/failed-commands.py <xcresult>
```

That is the build log's own tree of steps with everything that succeeded pruned out, so each
failure arrives with the target and command it came from. No failures under a failed build means
the build died outside of any step, and only the log says how.

Long step names and diagnostic messages are truncated. When those are what you
need, ask for the step they are in using `--step`. For example:
`failed-commands.py <xcresult> --step 'SwiftCompile'` reports detailed
information on any failing step whose title contains "SwiftComiple".

## Once you have the failure

Fix and rebuild. Examine the source code at the failure location and attempt to
fix the root cause. Have the main agent build again by invoking the
`build-webkit:xcode` skill with the same parameters it used before.

To narrow a failure that spans targets, the build skill can call build-webkit
using the `--only` argument, which selects a non-default Xcode scheme. For
example, `build.sh --only=WebCore` builds WebCore and skips the rest of the
stack. By convention, there is one scheme for each library, so `WTF`,
`bmalloc`, `JavaScriptCore`, `WebCore`, `WebKitLegacy`, and `WebKit` are all
available schemes.
