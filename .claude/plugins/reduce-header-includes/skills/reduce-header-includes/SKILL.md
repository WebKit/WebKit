---
name: reduce-header-includes
description: Use when the user wants to remove unused #include directives from a WebKit source directory (bmalloc, WTF, JavaScriptCore, WebCore, WebKit) to reduce build time. Runs clang-include-cleaner with WebKit-specific false-positive filters, ranks headers by ninja-deps fan-out, and fixes transitive breakage in a -k 0 loop.
user-invocable: true
allowed-tools: Bash, Read, Edit, Write, Agent
---

## 1. One-time setup

```sh
brew install llvm                         # → /opt/homebrew/opt/llvm/bin/clang-include-cleaner
BUILD=WebKitBuild/cmake-mac/Debug         # mac-dev-debug preset's binaryDir
cmake --preset mac-dev-debug              # emits $BUILD/compile_commands.json
cmake --build --preset mac-dev-debug      # generated/forwarding headers + .ninja_deps must exist before analysis
```

### 1a. Dump ninja dependency data (once per session, after a clean build)

```sh
ninja -C $BUILD -t deps > /tmp/ninja-deps.txt
```

This is the authoritative "which TUs read which headers" map for ranking and cascade prediction. It is reliable **only immediately after a successful build** — see §5a for staleness rules. Match headers by **basename** (`grep '/Foo\.h$'`), not full path: WTF deps appear via `WebKitBuild/.../Headers/wtf/` symlinks and JSC via `PrivateHeaders/JavaScriptCore/` copies, so the same header shows under multiple paths.

## 2. Prepare a filtered compile_commands.json

Homebrew clang can't read Apple-specific flags or Apple-built PCH, and the CMake textual prefix (`cmake_pch.hxx`) breaks standalone-header analysis. Build a filtered CDB once per session:

```sh
RES=$(xcrun clang -print-resource-dir)
mkdir -p /tmp/cdb-optimize
python3 - "$RES" <<'PY'
import json, re, sys
res = sys.argv[1]
src = json.load(open("WebKitBuild/cmake-mac/Debug/compile_commands.json"))
DROP = re.compile(r"-fcas-\S+|-fno-odr-hash-protocols|-clang-vendor-feature=\S+|"
                  r"-Wno-error=allocator-wrappers|-mllvm|-cas-friendly-debug-info|-Winvalid-pch")
out = []
for e in src:
    a, i = e["command"].split(), 0; keep = []
    while i < len(a):
        t = a[i]
        if t == "-Xclang" and i+1 < len(a) and (a[i+1] in ("-include-pch", "-include")
                or a[i+1].endswith((".pch", ".hxx")) or a[i+1].startswith("-fno-builtin-")):
            i += 2; continue
        if t == "--serialize-diagnostics": i += 2; continue
        if DROP.fullmatch(t): i += 1; continue
        keep.append(t); i += 1
    keep.append(f"-resource-dir={res}")
    e["command"] = " ".join(keep); out.append(e)
json.dump(out, open("/tmp/cdb-optimize/compile_commands.json", "w"))
PY
```

**Why strip `-Xclang -include -Xclang cmake_pch.hxx`:** the textual prefix transitively includes most project headers. When analyzing `Foo.h` as the *main file*, `#pragma once` doesn't guard the main file itself, so the prefix pulls in `Foo.h` once and then clang parses it again as the TU body → "redefinition" errors. Stripping the prefix is safe for `.cpp` files because they all `#include "config.h"` on line 1 anyway.

**Why inject `-resource-dir`:** without it, homebrew clang can't find `Availability.h`, so every `PLATFORM(...)`/`ENABLE(...)` macro resolves wrong and the suggestions are garbage.

## 3. Rank targets by impact (ninja deps)

Process headers highest-fan-out first — removing one include from a header read by 500 TUs is worth 500× one read by 3 TUs:

```sh
for h in <candidate headers>; do
  printf '%6d  %s\n' "$(grep -c "/$(basename "$h")\$" /tmp/ninja-deps.txt)" "$h"
done | sort -rn | head -40
```

Deps records *opened*, not *used* — it ranks reach, it cannot tell you what's removable. That's Pass A's job.

**Basename collision:** matching by basename merges same-named headers across projects (`bmalloc/Vector.h` + `wtf/Vector.h` reported as 405 when bmalloc's true reach is 4). When a count looks suspiciously high for the project, re-check with a parent-dir prefix: `grep -c "bmalloc/$(basename "$h")\$"`.

## 4. Pass A — Remove unused includes (`.h .cpp .c .mm .m`)

```sh
/opt/homebrew/opt/llvm/bin/clang-include-cleaner \
    --print=changes --disable-insert \
    --ignore-headers='config\.h,.*SoftLinking\.h,.*SPI\.h' \
    -p /tmp/cdb-optimize <file>
```

Output: `- "Header.h" @Line:N` per removable include.

**Headers (`.h`)** have no CDB entry; clang-include-cleaner interpolates flags from a sibling `.cpp`. Force the language and pre-include `config.h` (headers don't include it themselves):

```sh
/opt/homebrew/opt/llvm/bin/clang-include-cleaner \
    --print=changes --disable-insert \
    --ignore-headers='config\.h,.*SoftLinking\.h,.*SPI\.h' \
    --extra-arg-before=-xobjective-c++ \
    --extra-arg-before=-std=c++2b \
    --extra-arg=-includeconfig.h \
    -p /tmp/cdb-optimize <header.h>
```

The `-includeconfig.h` flag is **per-project** — bmalloc/libpas have no `config.h` (their headers self-include `BPlatform.h`/`pas_config.h`), so drop that flag there. Keep it for WTF/JSC/WebCore/WebKit.

If a header still won't parse standalone (heavy template/macro context, or it *is* one of the few headers `config.h` itself pulls in), skip it and note it — don't fight the tool.

**Never remove (all file types)**, even if the tool says so:
- `config.h`
- `*SoftLinking.h` (symbols via `SOFT_LINK_*` macros)
- The file's own header (`Foo.h` in `Foo.cpp`) — often only used via `DEFINE_ALLOCATOR`/`WTF_MAKE_*`
- `NeverDestroyed.h` when a `*_ALLOCATOR` / `LazyNeverDestroyed` macro/use is present
- `SIMDUTF.h`
- `*SPI.h`
- `<TargetConditionals.h>`, `<AvailabilityMacros.h>`, `<Availability.h>` — define `TARGET_OS_*`/`__MAC_*` macros that `#if` tests
- `<Foundation/Foundation.h>` and other ObjC framework umbrellas in `.mm`/`.m` — provide `NSString`/`NSBundle`/etc. via macros the tool can't trace
- Project macro-only foundations: bmalloc `BPlatform.h`/`BCompiler.h`/`BExport.h`/`BInline.h`; WTF `Platform.h`/`Compiler.h`/`ExportMacros.h`
- The flagged `#include` *is* the file's entire body for this platform — pure forwarding/portability shims (e.g., `pas_thread.h` is just `#include <pthread.h>` on non-Windows). The tool sees no local symbol use because re-export is the point.
- Any `#include` in a file that pulls `__has_include(<WebKitAdditions/...>)` — the additions header is internal-SDK-only and invisible to open-source tooling, so includes that exist to satisfy it (e.g., `AllocationCounts.h`'s `<atomic>`/`BExport.h`) read as unused.

**Never remove from `.h` files (OK to remove from `.c`/`.cpp`/`.m`/`.mm`):** these guard against *transitive* breakage in downstream TUs/platforms, which can't happen from a leaf file — there the file's own compile is the full verification.
- `<mach/*.h>` inside `#if BOS(DARWIN)`/`#if OS(DARWIN)` — needed on embedded even when macOS gets it transitively
- Any `#include` inside an `#if PLATFORM(...)`/`#if ENABLE(...)` block whose condition can differ on non-mac
- Any top-level `#include` whose only *use* is inside an `#if`/`#else` arm that's compiled out on mac (e.g., `BAssert.h`'s `Logging.h` — only referenced under `#if !BUSE(OS_LOG)`). The tool only sees the active config.
- Any `#include` whose only use is inside a `#define` macro body (e.g., `GigacageConfig.h`'s `<bit>` — `std::bit_cast` appears only in `#define g_gigacageConfig`). The tool doesn't analyze macro definitions, only expansions; if no expansion lands in the analyzed TU, it reports unused.
- `*Inlines.h` — template/inline bodies in the *target* header may need them at instantiation time in a downstream TU; the tool only sees instantiations in the analyzed TU.
- JSC split-inlines without the suffix: `JSCJSValueCell.h`, `JSCJSValueStructure.h`, `JSCJSValueBigInt.h`, `JSCellButterfly.h`. Same failure mode as `*Inlines.h` (out-of-class inline method *definitions*, not declarations) — removal links fine in the edited header but drops `JSValue::inherits()`/`structureOrNull()` defs from downstream TUs → undefined-symbol at link. Filter regex: `Inlines.*\.h|JSCJSValue(Cell|Structure|BigInt)\.h|JSCellButterfly\.h`.
- `wtf/text/StringHash.h` / `*Hash.h` when the target header defines a type used as a `HashMap`/`HashSet` key. The trait specialization (`DefaultHash<K>`) is needed at container *instantiation* in a downstream TU, not in the key header itself, so the tool reports it unused. (Observed: `RegExpKey.h` → `PackedRefPtr<StringImpl>` static_assert + `GenericHashTraits` undefined.)

**Verify by exported symbol, not header basename.** Before applying a removal of `Foo.h`, grep the target file for the names `Foo.h` *declares*, not just the string "Foo" — header name and type name often differ (`ArgList.h` → `MarkedArgumentBuffer`, `HashSet.h` → `UncheckedKeyHashSet`). Quick extractor:

```sh
grep -hoE '^\s*(class|struct|enum class|using|typedef|#define)\s+\w+' <Foo.h> | awk '{print $NF}' | sort -u
```

Then `grep -v '#include' <target> | grep -wF -f -` for each. If any hit, it's a FP.

Apply removals with the Edit tool (one line per removal). Do **not** use `--edit` — the never-remove filter must be applied manually.

### 4c. Leaf-file sweep (`.cpp .c .mm .m`, unranked)

After the §3-ranked header pass, sweep *all* implementation files in the target directory. Leaf TUs have fan-out 0 so they never surface via §3, but every TP is a free win with **zero cascade risk** — only the file's own compile needs to pass. PR #62576 found 38 removals in libpas `.c` files this way that the header-ranked pass missed entirely.

macOS `xargs -I{}` truncates long command lines, so write a helper once and pass filenames as `$1`:

```sh
cat > /tmp/run-cleaner-cpp.sh <<'EOF'
#!/bin/sh
out=$(/opt/homebrew/opt/llvm/bin/clang-include-cleaner --print=changes --disable-insert \
  --ignore-headers='config\.h,.*SoftLinking\.h,.*SPI\.h' \
  -p /tmp/cdb-optimize "$1" 2>/dev/null)
[ -n "$out" ] && printf '=== %s ===\n%s\n' "$1" "$out"
EOF
chmod +x /tmp/run-cleaner-cpp.sh

find Source/<dir> \( -name '*.c' -o -name '*.cpp' -o -name '*.m' -o -name '*.mm' \) -print0 \
  | xargs -0 -n1 -P8 /tmp/run-cleaner-cpp.sh > /tmp/leaf-results.txt
grep '^- ' /tmp/leaf-results.txt | sort | uniq -c | sort -rn | head
```

(For headers, copy to `/tmp/run-cleaner-header.sh` and add the §4 `--extra-arg-before=-xobjective-c++ --extra-arg-before=-std=c++2b --extra-arg=-includeconfig.h` flags.)

Apply only the **"all file types"** never-remove list above — the "headers only" filters don't apply here. Then `-k 0` build; any failure is a per-file revert, not a cascade.

## 5. Pass B — Add missing direct includes (`.cpp .c .mm .m` only)

**Pass B is ~4× noisier than Pass A** (bmalloc dry-run: ~20% precision vs ~70%). Default to running it only as the *repair* tool inside the §6 fix-loop, not as a blanket pre-pass. If you do run it broadly, apply the never-insert filter and path normalization aggressively.

```sh
/opt/homebrew/opt/llvm/bin/clang-include-cleaner \
    --print=changes --disable-remove \
    -p /tmp/cdb-optimize <file>
```

Output: `+ "Header.h"` / `+ <wtf/Header.h>` per missing direct include.

**Never run Pass B on `.h` files** — adding includes to headers bloats the transitive graph, which is what we're fighting.

**Normalize paths before inserting.** The tool emits `-I`-relative paths; WebKit uses quoted basenames. Strip leading `bmalloc/`, `libpas/src/libpas/`, `JavaScriptCore/`, `WebCore/`, `wtf/` (when inside WTF) so `+ "libpas/src/libpas/pas_lock.h"` becomes `#include "pas_lock.h"`.

**Never insert**:
- `wtf/Forward.h`, `wtf/Platform.h`, `wtf/Compiler.h`, `wtf/Assertions.h`, `wtf/ExportMacros.h` — provided by `config.h`
- bmalloc `BPlatform.h`/`BCompiler.h`/`BExport.h`/`BInline.h`/`BAssert.h` — bmalloc's `config.h`-equivalents
- Underscore-prefixed system internals (`<_strings.h>`, `<_stdio.h>`, …) — use the public header instead
- `<sys/qos.h>`, `<sys/_types/*.h>` — implementation details; use `<dispatch/dispatch.h>` / `<cstddef>` etc.
- `cmake_pch.hxx` or any prefix/PCH header
- A header that's already included via the file's own `.h`

Insertion point: after `#include "config.h"` and the file's own header, alphabetically within the existing group. In `.mm`/`.m`, use `#import` for ObjC framework headers (`<Foundation/...>`, `<WebKit/...>`), `#include` for everything else.

### 5a. Predict the cascade before rebuilding (optional shortcut)

After removing `#include "X.h"` from header `H`, the candidate-casualty set is:

```sh
awk -v H="/$(basename H)" -v X="/$(basename X.h)" '
  /: #deps /        { tu=$1; hasH=0; hasX=0 }
  $1 ~ H"$"         { hasH=1 }
  $1 ~ X"$"         { hasX=1 }
  hasH && hasX && tu { print tu; tu="" }
' /tmp/ninja-deps.txt | head
```

This **over-predicts** (flat list, no include-tree — can't tell "via H" from "via some other path") but never under-predicts for this platform/config. Run Pass B on those TUs first instead of waiting 10–15 min for `-k 0` to find them. If the set is ≳10 TUs, that's the §6 escape-hatch signal up front.

**Staleness:** `/tmp/ninja-deps.txt` reflects the *last successful compile* per TU. After edits it's stale-but-conservative (removed includes still listed → over-predicts, which is safe). Re-dump only after a clean `-k 0` build; don't re-dump mid-loop.

## 6. Build-and-fix loop

```sh
cmake --build --preset mac-dev-debug -- -k 0 2>&1 | tee /tmp/build.log
```

Run with `dangerouslyDisableSandbox: true`, `run_in_background: true`. Monitor:

```sh
grep -E "error:" /tmp/build.log | head -40
```

If `-k 0` reports exactly 1 `FAILED:` and it's `LLInt{Settings,Offsets}Extractor.cpp.o`, that target *generates* headers the rest of JSC depends on, so ninja can't continue past it. Fix that one error and re-run; the next round will surface the real cascade.

For each `use of undeclared identifier 'X'` / `unknown type name 'X'` / `incomplete type 'X'` / `no member named 'X'`:

1. **Identify the real TU.** If the error is in `UnifiedSourceN.cpp`/`.mm`, open the unified wrapper and find which constituent `.cpp` owns the failing line.
2. **Expect casualties in files you didn't edit.** Removing an include from a header, or from an earlier sibling in a unified bundle, breaks downstream files that were leaning on it transitively. This is normal — do not revert.
3. **Find the providing header.** `git grep -nE "^(class|struct|enum class|using) X\b" -- 'Source/**/*.h' | head`. Prefer the header with the *definition*, not a forward declaration.
4. **Add `#include "Provider.h"` to the failing real `.cpp`** — never to the unified wrapper. If the failing line is in a `.h` (the type appears in a signature/member), add the include to *that* `.h` — this is the IWYU fix; the header was leaning on a transitive path you just cut.
5. Re-build with `-k 0`. Repeat until clean.

**Escape hatch:** if one removed header line causes an unbounded cascade (≳10 TUs all needing the same add), that header was a de-facto umbrella — restore that single removal and move on. Known JSC umbrellas (skip removals here outright): `JSCInlines.h`, `JSCellInlines.h`, `JSCJSValueInlines.h`, `Lookup.h`, `ObjectAllocationProfile.h` — each fans out to all WebCore JS bindings.

## 7. Cross-platform fallout (after PR)

Local CMake only builds macOS. EWS-only breaks from removed includes that other platforms need directly:

- `<mach/mach.h>` — embedded Darwin (ios/tv/watch/vision) for `mach_task_self`/`kern_return_t`; macOS gets it transitively via frameworks
- `<mach/task_info.h>` — `task_vm_info_data_t`/`TASK_VM_INFO_COUNT` (does **not** provide `mach_task_self` — need `<mach/mach.h>` too)
- `<sys/file.h>` — Linux (WPE/GTK) for `flock()`/`LOCK_*`
- `<wtf/NeverDestroyed.h>` — `LazyNeverDestroyed<T>`; forward decl insufficient
- `<wtf/Function.h>` — `WTF::Function<>` on Linux; not pulled in transitively there

EWS-only breaks: re-add the include in the failing TU, guarded by the same `#if` the failing platform uses.
