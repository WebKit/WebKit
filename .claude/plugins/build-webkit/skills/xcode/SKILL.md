---
name: xcode
description: Use when the user wants to build WebKit on Apple platforms, where Xcode is the default build system. Builds with build-webkit, captures the results in an xcresult bundle, and reports the errors out of it, so tens of thousands of lines of build log don't pollute the conversation.
user-invocable: true
allowed-tools: Bash(${CLAUDE_SKILL_DIR}/scripts/build.sh *)
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
any other way: when Claude Code's sandbox is on, build.sh turns Xcode's own sandboxing off, and
changing that setting invalidates what is there.

## Read what failed

Run the `build-webkit:xcresult` scheme using the .xcresult bundle produced
by build-webkit. That skill may attempt a fix; once it does, rebuild with the
same parameters. To investigate an error in a particular target, rebuild one
scheme with `--only`, e.g. `build.sh --only=WebCore`. By convention, there is
one scheme for each library, so `WTF`, `bmalloc`, `JavaScriptCore`, `WebCore`,
`WebKitLegacy`, and `WebKit` are all available schemes.
