---
name: xcode
description: Use when the user wants to build WebKit on Apple platforms with Xcode (the default build tool there). Builds with `xcodebuild`, captures results in an xcresult bundle, and analyzes via `xcresulttool` so verbose build logs don't pollute the conversation.
user-invocable: true
allowed-tools: Bash, Read, Edit, Write, Agent
---

## When to use

The user wants to either (a) get an initial build passing or (b) fix new build issues after making changes. The build log produces tens of thousands of lines that will pollute context; the xcresult bundle has the same information in structured form.

## 1. Build and capture xcresult

From the repo's working tree:

```sh
set -o pipefail
Tools/Scripts/build-webkit --debug --result-bundle-path=/tmp/wk-build-N.xcresult ENABLE_USER_SCRIPT_SANDBOXING=NO DISABLE_TASK_SANDBOXING=YES OTHER_SWIFT_FLAGS='$(inherited) -disable-sandbox' 2>&1 | Tools/Scripts/filter-build-webkit > /tmp/wk-build-N.log 2>&1 ;
echo "exit=$?"
```

- Use a fresh `/tmp/wk-build-<n>.xcresult` path each iteration — `xcodebuild` refuses to overwrite an existing bundle, and a failed run can leave one behind (see §4).
- Redirect the filtered output to a file rather than letting it reach stdout. Anything on stdout becomes the tool result and lands in context, which is what this skill exists to avoid. The xcresult bundle holds everything needed to analyze; `/tmp/wk-build-<n>.log` is there for the user to tail or for a targeted `grep`.
- Use build-webkit's own `--result-bundle-path=<path>` flag, NOT the raw `-resultBundlePath <path>` passthrough. build-webkit splices its own flags out of `@ARGV`, but forwards leftover args to a second, out-of-band `build-imagediff` invocation (`build-webkit:405`). A passed-through `-resultBundlePath` therefore reaches a second `xcodebuild`, which fails with `error: Existing file at -resultBundlePath`. That failure is not cosmetic: build-webkit exits with imagediff's status (`build-webkit:407-409`), so it reports failure even when the main build succeeded.
- `set -o pipefail` with `$?`, not `${PIPESTATUS[0]}` — this environment's shell is zsh, whose arrays are 1-based, so `${PIPESTATUS[0]}` is always empty. `pipefail` works in both bash and zsh.
- **Run clean builds in the background.** A full WebKit build takes 30-45 minutes, well past the Bash tool's 15-minute cap, so a foreground call will time out. Launch with `run_in_background: true` and wait for the completion notification — do not poll with `sleep`, which burns turns for no benefit. Incremental rebuilds after a one-file fix are usually under a minute, so only clean builds need this.
- `ENABLE_USER_SCRIPT_SANDBOXING=NO`, `DISABLE_TASK_SANDBOXING=YES`, and `-disable-sandbox` to avoid conflicting with Claude's own sandbox. Without `DISABLE_TASK_SANDBOXING=YES`, individual build tasks stay sandboxed and steps like `CoreMLModelCompile` fail. (These are build settings, so adding them to an existing build directory invalidates its products and forces one full rebuild.)
- Exit code 65 is the typical "build failed" code. 0 means success.
- There are different configuration (debug, release) and platform flags (iOS, etc.) available. Run `Tools/Scripts/build-webkit -h` for usage info.
- Platform/SDK flags must use build-webkit's own spelling, e.g. `--sdk=iphoneos27.0.internal` or `--sdk iphoneos27.0.internal`. A bare `-sdk <name>` is not recognized by webkitdirs (`webkitdirs.pm:996`), so it passes through to xcodebuild while WebKit's own platform and build-directory logic still resolves to the host platform.

## 2. Get high-level error summary

```sh
xcrun xcresulttool get build-results --path /tmp/wk-build-N.xcresult
```

Returns JSON with `status`, `errorCount`, `warningCount`, and arrays of `errors`/`warnings`. Each issue has:
- `issueType`: observed values are `Semantic Issue`, `Swift Compiler Error`, `Error`, and `Uncategorized` in `errors`; `Warning` and `Target Integrity` in `warnings`. Don't filter on a specific spelling — the useful invariant is that everything except `Uncategorized` carries a `sourceURL`, and `Uncategorized` omits the key entirely.
- `message`: One-line description.
- `sourceURL`: A `file://` URL with the file path and line/column anchored in the URL fragment (`#StartingLineNumber=...`).

If the message is uncategorized (e.g. `"Command Ld failed with a nonzero exit code"`), the build-results summary won't include the actual compiler/linker error text — drop to step 3.

To pretty-print the status, group errors by file, and de-duplicate repeated messages:

```sh
xcrun xcresulttool get build-results --path /tmp/wk-build-N.xcresult \
  | python3 -c "
import json, sys, re, collections
d = json.load(sys.stdin)
print('status:', d.get('status'), '| title:', d.get('actionTitle'), '| errors:', d['errorCount'], '| warnings:', d['warningCount'])
byfile = collections.Counter()
for e in d.get('errors', []):
    m = re.match(r'file://([^#]*)', e.get('sourceURL', ''))
    byfile[(m.group(1).split('/')[-1] if m else '(no file)')] += 1
print('--- errors by file:')
for f, n in byfile.most_common():
    print(f'  {n:3d}  {f}')
print('--- distinct messages:')
seen = set()
for e in d.get('errors', []):
    msg = e.get('message', '')[:250]
    if msg not in seen:
        seen.add(msg)
        print(' *', e.get('issueType'), '|', msg)
"
```

Group by file before reading any source: a batch of errors is often one root cause. De-duplicating
messages matters too, since one bad declaration can produce the same message at a dozen call
sites.

For the exact line of a specific error, unquote its `sourceURL` and read the
`StartingLineNumber` fragment:

```sh
xcrun xcresulttool get build-results --path /tmp/wk-build-N.xcresult \
  | python3 -c "
import json, sys, urllib.parse
for e in json.load(sys.stdin).get('errors', []):
    u = e.get('sourceURL', '')
    path = urllib.parse.unquote(u.split('#')[0].removeprefix('file://'))
    frag = dict(p.split('=') for p in u.split('#')[1].split('&') if '=' in p) if '#' in u else {}
    print(e.get('message', '')[:100], '|', path, '| line', frag.get('StartingLineNumber'))
"
```

## 3. Get the structured build log (for "Uncategorized" errors and ordering)

For uncategorized linker/process errors, dig into the full build log:

```sh
xcrun xcresulttool get log --path /tmp/wk-build-N.xcresult --type build
```

This output is large (tens of KB). **Do not read the whole thing into context.** Pipe to a targeted grep:

```sh
xcrun xcresulttool get log --path /tmp/wk-build-N.xcresult --type build \
  | grep -nE "error:|Undefined|symbol not found|failed with a nonzero exit code|^Ld " | head -40
```

`failed with a nonzero exit code` is the phrasing for a build **script phase** that exited
non-zero (e.g. `Command PhaseScriptExecution failed with a nonzero exit code`). The message
itself says nothing about the cause, so grep with `-n` and then read the lines immediately
*before* that line number — a script's own stderr is whatever it printed on the way out:

```sh
xcrun xcresulttool get log --path /tmp/wk-build-N.xcresult --type build \
  | grep -n -B40 "failed with a nonzero exit code" | head -60
```

To see the build *order* and which targets ran (useful when diagnosing missing target dependency edges):

```sh
xcrun xcresulttool get log --path /tmp/wk-build-N.xcresult --type build --compact \
  | python3 -c "
import json, sys
data = json.load(sys.stdin)
def walk(obj, depth=0):
    if isinstance(obj, dict):
        if obj.get('title'):
            print('  '*depth + obj['title'][:120])
        for k, v in obj.items():
            if k in ('subsections', 'children'):
                for c in v: walk(c, depth+1)
walk(data)
" | head -80
```

If you see many `Run custom shell script 'Generate ...'` entries followed by a `Link <foo>` with no intervening `CompileC` steps, a target-dependency edge is missing — the linker is running before the target it depends on has compiled.

## 4. Misleading failures

**Judge success from the xcresult, not the log.** Both directions mislead:

- The log can print `Build Succeeded` while build-webkit still exits non-zero, because it
  exits with the status of its out-of-band `build-imagediff` run (§1).
- An xcresult bundle that no real build wrote reports `errors: 0`.

The reliable check is `status: succeeded` **plus** a real `actionTitle`, e.g.
`Build "Everything up to WebKit + Tools"`. An `actionTitle` of `(Transient Testing)` with
`status: notRequested` and `startTime == endTime` means the bundle is empty and you are
reading nothing.

**`Existing file at -resultBundlePath` means a previous run already created that path — it is
not itself the underlying failure.** A run that dies early still leaves an empty
`(Transient Testing)` bundle behind, so the *next* run with the same path fails immediately
with exit 64 and that single line of output. Retry with a genuinely new path, then read the
real error the first run produced.

A common real error underneath is `xcodebuild: error: '<path>.xcworkspace' does not exist.`
`WebKitBuild/Workspace` records the workspace from the last build, so it goes stale whenever
that workspace moves or is renamed. `rm -rf WebKitBuild/<Config>-<platform>` does NOT clear
it, because the file lives one level up in `WebKitBuild/`. Check it and either repoint or
reset to the default:

```sh
cat WebKitBuild/Workspace
Tools/Scripts/set-webkit-configuration --workspace <path to the .xcworkspace you want>
# or drop the override entirely:
Tools/Scripts/set-webkit-configuration --reset   # also resets configuration; or: rm WebKitBuild/Workspace
```

Passing `--workspace ""` does not clear the file.

## 5. Fix-build loop

1. Build (§1) → `get build-results` (§2). If `errors: 0` and `status: succeeded`, done.
2. `sourceURL` points at the offending line for semantic errors; for `Uncategorized` ones drop to `get log` with a targeted grep (§3).
3. Fix, then rebuild with a **new** xcresult path.

## 6. Build a single target to narrow down errors

When errors come from many targets, narrow to one using the `--only` argument:

```sh
Tools/Scripts/build-webkit --only JavaScriptCore --result-bundle-path=/tmp/wk-build-N.xcresult > /dev/null 2>&1
```

`--only` builds a specific scheme. List them all with `xcodebuild -workspace WebKit.xcworkspace -list`. Some common single-project schemes are:

- WTF
- JavaScriptCore
- WebGPU
- WebCore
- WebKit

