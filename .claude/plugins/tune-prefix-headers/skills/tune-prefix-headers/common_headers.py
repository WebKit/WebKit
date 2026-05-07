#!/usr/bin/env python3
"""Per-source-directory common-header analyzer for WebKit CMake builds.

For every source directory, find headers transitively included by >=THRESHOLD
of that directory's source files (via `ninja -t deps`), then rank directories
by (sum of header bytes) * (file count) -- the bytes-parsed payoff if those
headers moved into a specialized prefix/PCH.

See the accompanying SKILL.md for invocation and interpretation.
"""
import argparse
import glob
import math
import os
import re
import subprocess
import sys
from collections import defaultdict

TARGET_RE = re.compile(r"^(\S+): #deps")
CMAKEFILES_RE = re.compile(r"/CMakeFiles/[^/]+\.dir/")
UNIFIED_RE = re.compile(r"/UnifiedSource[^/]*\.(?:cpp|mm|c|m)$")
INC_RE = re.compile(r'^#include\s+"([^"]+\.(?:cpp|mm|c|m))"')

# Ordered: PAL is under WebCore/ on disk, WebKitLegacy before WebKit, so check
# more-specific prefixes first.
PROJECT_PREFIXES = [
    ("bmalloc",        ("Source/bmalloc/",)),
    ("WTF",            ("Source/WTF/",)),
    ("PAL",            ("Source/WebCore/PAL/", "PAL/DerivedSources/")),
    ("JavaScriptCore", ("Source/JavaScriptCore/", "JavaScriptCore/DerivedSources/")),
    ("WebCore",        ("Source/WebCore/", "WebCore/DerivedSources/")),
    ("WebKitLegacy",   ("Source/WebKitLegacy/", "WebKitLegacy/DerivedSources/")),
    ("WebKit",         ("Source/WebKit/", "WebKit/DerivedSources/")),
    ("WebGPU",         ("Source/WebGPU/",)),
    ("WebDriver",      ("Source/WebDriver/",)),
    ("WebInspectorUI", ("Source/WebInspectorUI/",)),
]

CANON_RE = re.compile(r"(?:PrivateHeaders|Headers)/([^/]+/[^/]+)$")
SOURCE_RE = re.compile(r"(?:^|/)Source/([^/]+/.+)$")
DERIVED_RE = re.compile(r"(?:^|/)([^/]+/DerivedSources/.+)$")
CACHE_SRC_RE = re.compile(r"^(?:CMAKE_HOME_DIRECTORY|WebKit_SOURCE_DIR)[^=]*=(.+)$")


def project_of(src_path):
    for label, prefixes in PROJECT_PREFIXES:
        if any(src_path.startswith(p) for p in prefixes):
            return label
    return None


def canon_header(h):
    """Collapse forwarding-header aliases so subtraction works:
    .../PrivateHeaders/WebCore/Document.h == Source/WebCore/dom/Document.h."""
    m = CANON_RE.search(h)
    if m:
        return m.group(1)
    m = DERIVED_RE.search(h)
    if m:
        return m.group(1)
    m = SOURCE_RE.search(h)
    if m:
        fw, _, rest = m.group(1).partition("/")
        return fw + "/" + os.path.basename(rest)
    return os.path.basename(h)


def find_source_dir(build_dir):
    cache = os.path.join(build_dir, "CMakeCache.txt")
    try:
        with open(cache) as f:
            for line in f:
                m = CACHE_SRC_RE.match(line)
                if m:
                    return os.path.abspath(m.group(1))
    except OSError:
        pass
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"], text=True).strip()
    except subprocess.CalledProcessError:
        sys.exit("error: could not determine source dir; pass --source-dir")


def find_build_dir(src_dir_hint):
    roots = [src_dir_hint] if src_dir_hint else []
    try:
        roots.append(subprocess.check_output(
            ["git", "rev-parse", "--show-toplevel"], text=True).strip())
    except subprocess.CalledProcessError:
        pass
    for root in roots:
        for cand in sorted(glob.glob(os.path.join(root, "WebKitBuild", "*"))):
            if os.path.exists(os.path.join(cand, ".ninja_deps")):
                return cand
            for sub in sorted(glob.glob(os.path.join(cand, "*"))):
                if os.path.exists(os.path.join(sub, ".ninja_deps")):
                    return sub
    sys.exit("error: no build dir with .ninja_deps under WebKitBuild/; "
             "pass --build-dir")


def is_sdk(p):
    return ("/Xcode" in p or "/CommandLineTools/" in p
            or "WebKitLibraries/SDKs" in p or p.startswith("/usr/"))


def obj_to_source(tgt):
    """Map a ninja .o target to (framework_src_root, source_file_rel_path)."""
    if tgt.endswith(".o"):
        tgt = tgt[:-2]
    m = CMAKEFILES_RE.search(tgt)
    if not m:
        return None, None
    fw_root = tgt[:m.start()]  # e.g. Source/JavaScriptCore
    rest = tgt[m.end():].replace("__", "..")
    src_rel = os.path.normpath(os.path.join(fw_root, rest))
    return fw_root, src_rel


def resolve_on_disk(src_rel, src_dir, build_dir):
    for base in (src_dir, build_dir):
        p = os.path.join(base, src_rel)
        if os.path.exists(p):
            return p
    return None


def expand_unified(wrapper_path, fw_root, src_dir, build_dir):
    """Yield constituent source-relative paths from a UnifiedSource wrapper.
    Constituent "X.cpp" may live under Source/<fw>/ or BUILD/<fw>/DerivedSources/."""
    fw_name = os.path.basename(fw_root)
    derived = os.path.join(fw_name, "DerivedSources")
    try:
        with open(wrapper_path, errors="ignore") as f:
            for line in f:
                m = INC_RE.match(line)
                if not m:
                    continue
                inc = m.group(1)
                cand_src = os.path.normpath(os.path.join(fw_root, inc))
                if os.path.exists(os.path.join(src_dir, cand_src)):
                    yield cand_src
                    continue
                cand_gen = os.path.normpath(os.path.join(derived, inc))
                if os.path.exists(os.path.join(build_dir, cand_gen)):
                    yield cand_gen
                    continue
                yield cand_src
    except OSError:
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir")
    ap.add_argument("--source-dir",
                    help="WebKit checkout root (default: read from "
                         "<build-dir>/CMakeCache.txt)")
    ap.add_argument("--threshold", type=float, default=0.75)
    ap.add_argument("--root", action="append", default=[])
    ap.add_argument("--group", action="append", default=[],
                    help="REGEX=LABEL: source paths matching REGEX are grouped "
                         "under virtual directory LABEL instead of their dirname")
    ap.add_argument("--no-default-groups", action="store_true",
                    help="disable the built-in WebCore/JSBindings merge")
    ap.add_argument("--min-files", type=int, default=8)
    ap.add_argument("--include-sdk", action="store_true")
    ap.add_argument("--tiered", action="store_true",
                    help="two-tier mode: per-PROJECT common set first, then "
                         "per-subdir common set MINUS the parent project set")
    ap.add_argument("--top", type=int, default=5)
    ap.add_argument("--out", default="/tmp/common_headers")
    args = ap.parse_args()

    build_dir = os.path.abspath(args.build_dir) if args.build_dir \
        else find_build_dir(args.source_dir)
    src_dir = os.path.abspath(args.source_dir) if args.source_dir \
        else find_source_dir(build_dir)
    print(f"[common_headers] source dir: {src_dir}", file=sys.stderr)
    print(f"[common_headers] build dir:  {build_dir}", file=sys.stderr)

    roots = args.root or ["Source/"]
    DEFAULT_GROUPS = [
        # Generated JS*.cpp bindings, generated bindings/js/, and hand-written
        # Source/WebCore/bindings/js/ all share WebCoreJSBindingsPrefix.h.
        r"^WebCore/DerivedSources/JS.*\.cpp$=WebCore/JSBindings",
        r"^WebCore/DerivedSources/bindings/js/=WebCore/JSBindings",
        r"^Source/WebCore/bindings/js/=WebCore/JSBindings",
    ]
    group_specs = args.group + ([] if args.no_default_groups else DEFAULT_GROUPS)
    groups = []
    for g in group_specs:
        rx, _, label = g.partition("=")
        groups.append((re.compile(rx), label))
    os.makedirs(args.out, exist_ok=True)

    def short(p):
        return p.replace(build_dir + "/", "").replace(src_dir + "/", "")

    def assign_dir(src_path):
        for rx, label in groups:
            if rx.search(src_path):
                return label
        return os.path.dirname(src_path)

    # ---- stream ninja deps, attribute each TU to one or more source files ----
    print(f"[common_headers] reading ninja deps from {build_dir} ...", file=sys.stderr)
    records = []  # (frozen source tuple, dep set) -- constituents share one set
    cur_set = None
    proc = subprocess.Popen(["ninja", "-t", "deps"], cwd=build_dir,
                            stdout=subprocess.PIPE, text=True)
    for line in proc.stdout:
        if line.startswith("    "):
            if cur_set is not None:
                cur_set.add(line[4:].rstrip("\n"))
            continue
        cur_set = None
        m = TARGET_RE.match(line)
        if not m:
            continue
        tgt = m.group(1)
        if tgt.endswith(".pch") or "cmake_pch" in tgt:
            continue
        fw_root, src_rel = obj_to_source(tgt)
        if src_rel is None:
            continue
        if UNIFIED_RE.search(src_rel):
            wrapper = resolve_on_disk(src_rel, src_dir, build_dir)
            sources = list(expand_unified(wrapper, fw_root, src_dir, build_dir)) if wrapper else []
        else:
            sources = [src_rel]
        if not sources:
            sources = [src_rel]
        cur_set = set()
        records.append((sources, cur_set))
    proc.wait()

    # Merge: same source path across multiple targets -> union of dep sets.
    tu_deps = {}
    for sources, deps in records:
        for s in sources:
            if s in tu_deps:
                tu_deps[s] = tu_deps[s] | deps
            else:
                tu_deps[s] = deps

    # ---- header sizes (cached) ----
    size_cache = {}

    def hsize(h):
        if h not in size_cache:
            full = h if os.path.isabs(h) else os.path.join(build_dir, h)
            try:
                size_cache[h] = os.path.getsize(full)
            except OSError:
                size_cache[h] = 0
        return size_cache[h]

    if args.tiered:
        run_tiered(args, tu_deps, roots, assign_dir, hsize, short)
        return

    # ---- group by directory ----
    by_dir = defaultdict(list)
    for s, deps in tu_deps.items():
        d = assign_dir(s)
        if any(d.startswith(r.rstrip("/")) for r in roots):
            by_dir[d].append(deps)

    # ---- per-directory analysis ----
    results = []  # (payoff, d, n_files, common_list)
    for d, tus in by_dir.items():
        n = len(tus)
        if n < args.min_files:
            continue
        thr = math.ceil(n * args.threshold)
        counts = defaultdict(int)
        for deps in tus:
            for h in deps:
                counts[h] += 1
        common = [(h, c) for h, c in counts.items()
                  if c >= thr and (args.include_sdk or not is_sdk(h))]
        total_size = sum(hsize(h) for h, _ in common)
        payoff = total_size * n
        results.append((payoff, d, n, thr, common, total_size))

    results.sort(reverse=True)

    # ---- summary table ----
    print(f"\n{len(results)} directories (>= {args.min_files} files each), "
          f"threshold >= {args.threshold:.0%}, roots: {', '.join(roots)}\n")
    print(f"{'payoff(MB·f)':>14} {'files':>6} {'≥thr':>5} {'hdrs':>6} {'totKB':>9}  directory")
    for payoff, d, n, thr, common, total_size in results:
        print(f"{payoff/1e6:>14.1f} {n:>6} {thr:>5} {len(common):>6} "
              f"{total_size/1024:>9.0f}  {d}")

    # ---- summary.tsv ----
    with open(os.path.join(args.out, "summary.tsv"), "w") as f:
        f.write("dir\tn_files\tn_common\ttotal_bytes\tpayoff\n")
        for payoff, d, n, thr, common, total_size in results:
            f.write(f"{d}\t{n}\t{len(common)}\t{total_size}\t{payoff}\n")

    # ---- per-dir detail files + top-N stdout blocks ----
    for i, (payoff, d, n, thr, common, total_size) in enumerate(results):
        rows = sorted(((hsize(h), c, h) for h, c in common), reverse=True)
        san = d.replace("/", "_")
        with open(os.path.join(args.out, f"{san}.txt"), "w") as f:
            for sz, c, h in rows:
                f.write(f"{sz}\t{c}\t{short(h)}\n")
            f.write(f"\n# total_headers\t{len(common)}\n")
            f.write(f"# total_size_bytes\t{total_size}\n")
            f.write(f"# n_files\t{n}\n")
            f.write(f"# payoff_bytes_x_files\t{payoff}\n")
        if i < args.top:
            print(f"\n=== {d}  ({n} files, ≥{thr}/{n}) ===")
            print(f"{'KB':>9} {'TUs':>5}  header")
            for sz, c, h in rows[:30]:
                print(f"{sz/1024:>9.1f} {c:>5}  {short(h)}")
            if len(rows) > 30:
                print(f"   ... +{len(rows)-30} more in {args.out}/{san}.txt")
            print(f"  total headers : {len(common)}")
            print(f"  total size    : {total_size/1024:.0f} KB ({total_size} bytes)")
            print(f"  files covered : {n}")
            print(f"  size × files  : {payoff/1e6:.1f} MB·files ({payoff} byte·files)")

    print(f"\nDetail files: {args.out}/<dir>.txt   Summary: {args.out}/summary.tsv",
          file=sys.stderr)


def run_tiered(args, tu_deps, roots, assign_dir, hsize, short):
    """Two-tier: project-level >=thr set, then per-subdir >=thr set minus parent."""
    os.makedirs(args.out, exist_ok=True)

    def filt(deps):
        return {h for h in deps if args.include_sdk or not is_sdk(h)}

    # Partition TUs by project and by (project, subdir).
    by_proj = defaultdict(list)
    by_sub = defaultdict(list)
    for s, deps in tu_deps.items():
        if "ThirdParty" in s:
            continue
        proj = project_of(s)
        if not proj:
            continue
        d = assign_dir(s)
        if roots and not any(d.startswith(r.rstrip("/")) or proj == r.rstrip("/")
                             for r in roots):
            continue
        f = filt(deps)
        by_proj[proj].append(f)
        by_sub[(proj, d)].append(f)

    def common_set(tus, thr_frac):
        n = len(tus)
        thr = math.ceil(n * thr_frac)
        cnt = defaultdict(int)
        for deps in tus:
            for h in deps:
                cnt[h] += 1
        return n, cnt, {h for h, c in cnt.items() if c >= thr}

    def dedup_rows(headers, cnt):
        """Collapse forwarding aliases; pick the largest representative path."""
        best = {}
        for h in headers:
            ck = canon_header(h)
            sz = hsize(h)
            cur = best.get(ck)
            if cur is None or sz > cur[1]:
                best[ck] = (h, sz, cnt[h])
        return sorted(best.values(), key=lambda r: -r[1])

    # ---- Tier 1: per project ----
    proj_common_canon = {}
    print("=" * 100)
    print(f"TIER 1 — per-PROJECT common headers (>= {args.threshold:.0%} of project TUs)")
    print("=" * 100)
    print(f"{'project':<16} {'TUs':>6} {'hdrs':>6} {'totKB':>8}")
    proj_order = [p for p, _ in PROJECT_PREFIXES if p in by_proj]
    for proj in proj_order:
        n, cnt, common = common_set(by_proj[proj], args.threshold)
        rows = dedup_rows(common, cnt)
        tot = sum(r[1] for r in rows)
        proj_common_canon[proj] = {canon_header(h) for h in common}
        print(f"{proj:<16} {n:>6} {len(rows):>6} {tot/1024:>8.0f}")
        with open(os.path.join(args.out, f"PROJECT_{proj}.txt"), "w") as f:
            for h, sz, c in rows:
                f.write(f"{sz:>9}  {c:>5}  {100*c/n:5.1f}%  {short(h)}\n")
            f.write(f"# n_tus {n}\n# n_headers {len(rows)}\n# total_bytes {tot}\n")

    print(f"\nPer-project top-{min(args.top*5,30)} headers by size "
          f"(full lists: {args.out}/PROJECT_*.txt):")
    for proj in proj_order:
        n, cnt, common = common_set(by_proj[proj], args.threshold)
        rows = dedup_rows(common, cnt)
        print(f"\n  [{proj}]  {n} TUs, {len(rows)} common headers, "
              f"{sum(r[1] for r in rows) / 1024:.0f} KB total")
        for h, sz, c in rows[:min(args.top * 5, 30)]:
            print(f"    {sz / 1024:7.1f} KB  {100 * c / n:5.1f}%  {short(h)}")

    # ---- Tier 2: per subdir, minus parent project set ----
    sub_results = []
    for (proj, sub), tus in by_sub.items():
        n = len(tus)
        if n < args.min_files:
            continue
        _, cnt, common = common_set(tus, args.threshold)
        parent = proj_common_canon.get(proj, set())
        delta = {h for h in common if canon_header(h) not in parent}
        rows = dedup_rows(delta, cnt)
        tot = sum(r[1] for r in rows)
        sub_results.append((tot * n, proj, sub, n, rows, tot))
    sub_results.sort(reverse=True)

    print()
    print("=" * 100)
    print(f"TIER 2 — per-SUBDIR delta (>= {args.threshold:.0%} of subdir TUs, "
          f"MINUS parent project's tier-1 set)")
    print("=" * 100)
    print(f"{'payoff(MB·f)':>12} {'files':>6} {'Δhdrs':>6} {'ΔKB':>7}  project / subdir")
    for payoff, proj, sub, n, rows, tot in sub_results:
        print(f"{payoff/1e6:>12.1f} {n:>6} {len(rows):>6} {tot/1024:>7.0f}  {proj:<14} / {sub}")

    with open(os.path.join(args.out, "tier2_summary.tsv"), "w") as f:
        f.write("project\tsubdir\tn_files\tdelta_hdrs\tdelta_bytes\tpayoff\n")
        for payoff, proj, sub, n, rows, tot in sub_results:
            f.write(f"{proj}\t{sub}\t{n}\t{len(rows)}\t{tot}\t{payoff}\n")

    print(f"\nTop {args.top} subdir deltas — header detail "
          f"(full lists: {args.out}/SUBDIR_*.txt):")
    for i, (payoff, proj, sub, n, rows, tot) in enumerate(sub_results):
        safe = re.sub(r"[^A-Za-z0-9]+", "_", f"{proj}__{sub}")
        with open(os.path.join(args.out, f"SUBDIR_{safe}.txt"), "w") as f:
            f.write(f"# project {proj}\n# subdir {sub}\n")
            for h, sz, c in rows:
                f.write(f"{sz:>9}  {c:>5}  {100*c/n:5.1f}%  {short(h)}\n")
            f.write(f"# n_files {n}\n# delta_headers {len(rows)}\n"
                    f"# delta_bytes {tot}\n# payoff {payoff}\n")
        if i >= args.top:
            continue
        print(f"\n  [{proj} / {sub}]  {n} files, Δ={len(rows)} hdrs, "
              f"{tot/1024:.0f} KB, payoff={payoff/1e6:.1f} MB·f")
        for h, sz, c in rows[:30]:
            print(f"    {sz/1024:7.1f} KB  {100*c/n:5.1f}%  {short(h)}")
        if len(rows) > 30:
            print(f"    ... +{len(rows)-30} more")

    print(f"\nTier-1 lists: {args.out}/PROJECT_*.txt   "
          f"Tier-2 lists: {args.out}/SUBDIR_*.txt   "
          f"Summary: {args.out}/tier2_summary.tsv", file=sys.stderr)


if __name__ == "__main__":
    main()
