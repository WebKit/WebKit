---
name: tune-prefix-headers
description: Use when choosing or auditing contents of a WebKit prefix header / PCH (e.g. WebCorePrefix.h, JavaScriptCorePrefix.h, or a specialized/chained prefix). Mines `ninja -t deps` from a CMake build to find headers included by >=75% of a directory's TUs, ranked by bytes-times-files re-parse payoff.
user-invocable: true
allowed-tools: Bash(python3:*), Bash(cmake:*), Bash(ninja:*), Read, Grep
---

## Prereq

`ninja -t deps` reads `.ninja_deps`, which only exists after a build. Any completed CMake+ninja build tree will do:

```sh
cmake --preset <preset-name>
cmake --build --preset <preset-name>
```

## Invoke

```sh
python3 "${CLAUDE_PLUGIN_ROOT}/skills/tune-prefix-headers/common_headers.py" \
    --build-dir WebKitBuild/<preset-name> \
    --root Source/WebCore --root WebCore/ \
    --threshold 0.75 --top 10
```

Flags:
- `--build-dir` — CMake build directory containing `.ninja_deps`. Default: first `WebKitBuild/*/` (or `WebKitBuild/*/*/`) that has one.
- `--source-dir` — WebKit checkout root. Default: read from `<build-dir>/CMakeCache.txt`.
- `--root` (repeatable) — only report directories under these prefixes. `Source/WebCore` covers checked-in sources; `WebCore/` covers `WebCore/DerivedSources/...` (build-relative).
- `--group REGEX=LABEL` (repeatable) — override dirname grouping; first match wins. **Built-in default** merges generated `WebCore/DerivedSources/JS*.cpp` + `WebCore/DerivedSources/bindings/js/` + hand-written `Source/WebCore/bindings/js/` into one virtual `WebCore/JSBindings` group (they share one bindings prefix). Pass `--no-default-groups` to see them separately.
- `--threshold` — fraction of a directory's files that must include a header for it to count. Use **`0.75`** for first-party code. Use **`0.60`** for `Source/ThirdParty/*`: those targets are built clean far more often than they're edited, so the incremental-rebuild penalty of a fatter prefix matters less and the clean-build win matters more.
- `--tiered` — two-tier mode for chained PCH: first compute the ≥thr set per **project** (bmalloc/WTF/JSC/PAL/WebCore/WebKit/WebKitLegacy/WebGPU) → `PROJECT_*.txt`; then per-subdir ≥thr set **minus** the parent project's set → `SUBDIR_*.txt` + `tier2_summary.tsv`. Use this when picking specialized prefix contents: tier-1 = base `*Prefix.h`, tier-2 = chained delta.
- `--min-files` — skip directories with fewer source files (default 8).
- `--include-sdk` — also count SDK/Xcode headers (off by default; you can't usefully prefix them beyond what the framework PCH already does).
- `--top N` — print N detail blocks to stdout (default 5).
- `--out` — per-directory detail files + `summary.tsv` land here (default `/tmp/common_headers`).

ThirdParty example:
```sh
python3 "${CLAUDE_PLUGIN_ROOT}/skills/tune-prefix-headers/common_headers.py" \
    --root Source/ThirdParty/ANGLE --threshold 0.60 --top 5
```

## Read the output

**Summary table** (sorted by payoff desc):
```
payoff(MB·f)  files  ≥thr   hdrs   totKB  directory
```
`payoff = (Σ bytes of ≥thr headers) × (file count)` — bytes re-parsed across the directory if those headers are *not* in a prefix. Biggest-payoff directories are the best specialized-prefix candidates.

**Detail block / `<out>/<dir>.txt`**: `bytes  TUs  header` rows sorted by size, then footer:
```
# total_headers     N
# total_size_bytes  Σ
# n_files           F
# payoff_bytes_x_files  Σ×F
```
That header list is your candidate `*Prefix.h` contents. Subtract anything already in the parent framework's PCH (`diff` against `WebCorePrefix.h` / `JavaScriptCorePrefix.h`); `--tiered` does this subtraction for you at the project level.

## Caveats

- **Unified-bundle inflation.** Each constituent `.cpp` inherits the *bundle's* full dep set, so the ≥75% bar is slightly easier to clear than a non-unified build would show. This is bias, not error — the bundle really does parse all of it.
- **Forwarding-header double-count.** `Source/WebCore/dom/Document.h` and `WebCore/PrivateHeaders/WebCore/Document.h` both appear; they're the same file via two `-I` paths. The byte total is inflated by the duplicate. Ignore when picking; including either once covers both. (`--tiered` collapses these aliases.)
- **SDK headers excluded** by default (paths matching Xcode/CommandLineTools/WebKitLibraries-SDKs/`/usr/`).
