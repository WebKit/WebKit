---
name: xcode
description: Use when the user wants to build WebKit on Apple platforms, where Xcode is the default build system. Builds with build-webkit, captures the results in an xcresult bundle, and reports the errors out of it, so tens of thousands of lines of build log don't pollute the conversation.
user-invocable: true
allowed-tools: Bash(${CLAUDE_SKILL_DIR}/scripts/build.sh:*), Bash(${CLAUDE_SKILL_DIR}/scripts/errors.py:*), Bash(${CLAUDE_SKILL_DIR}/scripts/failed-commands.py:*)
---

Build through these scripts rather than running `build-webkit` yourself: they keep the log out of
the conversation and report what failed from the xcresult bundle instead.

## Build

```sh
${CLAUDE_SKILL_DIR}/scripts/build.sh [build-webkit arguments]
```

Prints the path of the xcresult bundle, the path of the log, and build-webkit's exit status.
Arguments are build-webkit's own — `--debug`, `--only=<scheme>`, `--sdk=<name>`, and the rest of
`build-webkit -h`. A bare `-sdk` is not among them: it reaches xcodebuild, but WebKit's own
platform and build directory logic still resolves to the host. When an internal SDK is installed
build-webkit prefers it, and then builds `../Internal/Safari.xcworkspace` rather than
`WebKit.xcworkspace`.

Run clean builds with `run_in_background: true`: they take ~15-30 minutes, well past the Bash tool's
timeout. Wait for the completion notification rather than polling with `sleep`. Rebuilds after a
one file fix are usually under a minute, except the first one in a build directory that was built
any other way: the settings build.sh passes to turn off Xcode's sandboxing invalidate what's there.

## Read what failed

```sh
${CLAUDE_SKILL_DIR}/scripts/errors.py <xcresult>
```

Reports the build's status, how many errors each file has, and every distinct message with the
places it came from. Read those counts before opening any source file: a batch of errors is usually
one root cause, and one bad declaration repeats the same message at every call site.

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

## Fix and rebuild

Examine the source code at the failure location, fix the root cause, and build
again. Judge success from `errors.py` rather than from build.sh's exit status.

To narrow a failure that spans targets, rebuild one scheme with `--only`, e.g.
`build.sh --only=WebCore`. By convention, there is one scheme for each library,
so `WTF`, `bmalloc`, `JavaScriptCore`, `WebCore`, `WebKitLegacy`, and `WebKit`
are all available schemes.
