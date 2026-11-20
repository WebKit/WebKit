#!/usr/bin/env python3 -I

# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Check a -O0 (Debug) arm64/arm64e build for ARC / non-ARC ODR violations in inline functions.

Model: In an inline function, if we see an ObjC ownership operation on a non-local, then:
    * It must have been emitted by ARC, because non-ARC uses smart pointers
        * and smart pointer operations are out of line in debug builds
    * Therefore: It will be an ODR violation when the same function is compiled non-ARC

"Member" means any storage outside the function's stack frame: a data member, a global or static, or memory reached through a pointer.
  assigns-objc-member             ARC assigns an Objective-C pointer in a member (objc_storeStrong, objc_storeWeak,
                                  objc_initWeak, objc_copyWeak, objc_moveWeak) or reads a weak one (objc_loadWeak*)
  destroys-objc-member            ARC destroys an Objective-C pointer in a member: objc_storeStrong(&m, nil),
                                  which is also what an explicit m = nil compiles to, or objc_destroyWeak
  initializes-objc-member         ARC retains an object and stores it into a member (objc_retain*, _Block_copy,
                                  objc_alloc*, alloc/new/copy/mutableCopy results), allowing one trip through a
                                  frame slot
  passes-member-to-arc-only-code  a member's address is passed to a callee that exists only in ARC code (its
                                  mangled name has __strong/__weak/__autoreleasing) and uses it as ARC storage
  returns-autoreleased-new-object ARC autoreleases an object the function created (alloc/new/copy/mutableCopy/init
                                  family) to return it at +0, while non-ARC returns the same object at +1
  unclassified                    an address or callee the checker can't classify
Allow list:
  not-in-header           the function is declared in an implementation file, so it isn't duplicated ARC / non-ARC
  arc-only-header         the function is declared in a header with the standard guard
                          (#if !__has_feature(objc_arc) / #error), so no non-ARC file can include it
Exit status is 1 if there are findings other than the informational ones, and 2 if --build selects objects
compiled with optimization (from each object's command in compile_commands.json).

Usage:
  check-for-arc-odr-violations.py --build BUILD_DIR    every ARC and Swift object in BUILD_DIR/compile_commands.json
  check-for-arc-odr-violations.py --build FILE.json    every ARC and Swift object in FILE.json
  check-for-arc-odr-violations.py OBJ...               specific object files
For an Xcode build, generate-compile-commands writes compile_commands.json to the current directory. Xcode records
only clang commands in it, so Swift objects are not checked.
Requires Xcode's command line tools (nm, llvm-objdump, llvm-dwarfdump, c++filt).
"""
import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys


OWNERSHIP_IN_NAME = re.compile(r'U8__strong|U6__weak|U14__autoreleasing')
BLOCK_HELPER = re.compile(r'^___(copy|destroy)_helper_block_')
# Smart pointer implementations, whose job is to manage ARC storage. Matched on the mangled name, whose leading
# nested name is the function's own name, while a demangled name can start with a return type.
EXEMPT_MANGLED = re.compile(r'^_?_Z(?:N[KVRO]*)?3WTF\d+(RetainPtr\w*|RetainRef|WeakObjCPtr|BlockPtr|OSObjectPtr|adoptNS|'
                            r'adoptCF|adoptOSObject|retainPtr|protect|lazyInitialize|bridge_cast|bridge_id_cast|'
                            r'bridge_cast_to_id|dynamic_objc_cast|checked_objc_cast|dynamic_cf_cast|checked_cf_cast|'
                            r'dynamic_os_cast|checked_os_cast|DefaultOSObjectRetainTraits)(?=[IECD0-9]|$)')
SWIFT_NATIVE = re.compile(r'^_?\$s')  # Swift code is always ARC, so it has no non-ARC twin

ADDRESS_OPS = {  # runtime function -> argument registers that are addresses of ARC storage
    'objc_storeStrong': ['x0'], 'objc_initWeak': ['x0'], 'objc_storeWeak': ['x0'], 'objc_destroyWeak': ['x0'],
    'objc_loadWeak': ['x0'], 'objc_loadWeakRetained': ['x0'], 'objc_copyWeak': ['x0', 'x1'],
    'objc_moveWeak': ['x0', 'x1'],
}
# Calls that return a +1 object: CREATE makes a new one, RETAIN retains an existing one.
# Selector stubs keep the selector's colons; (?![a-z]) is Cocoa's method-family word boundary.
CREATE = re.compile(r'^(objc_alloc|objc_alloc_init|objc_opt_new|objc_opt_copy|'
                    r'objc_msgSend\$(alloc|new|copy|mutableCopy|init)(?![a-z])\S*)$')
RETAIN = re.compile(r'^(objc_retain\w*|objc_claimAutoreleasedReturnValue|objc_retainBlock|_Block_copy)$')
NOT_RETAIN = re.compile(r'^objc_retainAutorelease(ReturnValue)?$')  # these hand back a +0 value

FUNC = re.compile(r'^([0-9a-f]+) <(.+)>:$')
INSN = re.compile(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$')
RELOC = re.compile(r'^\s+([0-9a-f]+):\s+(ARM64_RELOC_\w+)\s+(\S+)')
MEM = re.compile(r'\[(\w+)(?:,\s*#(-?0x[0-9a-f]+|-?\d+))?\]')
FRAME_BASES = {'sp', 'x29', 'fp'}


def reg64(r):
    r = r.strip().rstrip(',')
    return 'x' + r[1:] if re.fullmatch(r'w\d+', r) else r


class Fn:
    def __init__(self, name, addr=0):
        self.name, self.addr, self.insns, self.relocs = name, addr, [], {}


def disassemble(obj):
    out = subprocess.run(['xcrun', 'llvm-objdump', '-d', '-r', '--no-show-raw-insn', obj],
                         capture_output=True, text=True, check=True).stdout
    if 'file format mach-o arm64' not in out:
        sys.exit(f'error: {obj} is not an arm64/arm64e object; this check only understands arm64 code')
    fns, cur = {}, None
    for line in out.splitlines():
        m = FUNC.match(line)
        if m:
            cur = fns[m.group(2)] = Fn(m.group(2), int(m.group(1), 16))
            continue
        if cur is None:
            continue
        m = RELOC.match(line)
        if m:
            cur.relocs[int(m.group(1), 16)] = (m.group(2), m.group(3))
            continue
        m = INSN.match(line)
        if m:
            ops = m.group(3).split(';')[0].strip()
            cur.insns.append((int(m.group(1), 16), m.group(2), ops))
    return fns


def offset_key(base, imm):
    return (base if base != 'fp' else 'x29', int(imm, 0) if imm else 0)


ARG_REGS = [f'x{i}' for i in range(8)] + ['x8']  # x8 carries the indirect result (sret) address


def analyze(fn, summaries):
    """Return (findings, storage_args) for one function.

    findings: (kind, address, detail) ownership operations on storage outside the frame.
    storage_args: indices of incoming argument registers (8 = x8, the sret address) that this
    function uses as the address of ARC storage, directly or by passing them to callees that do.
    summaries: storage_args of the other functions in the object (callee name without underscore).
    """
    findings, storage_args = [], set()
    targets = set()
    for addr, mn, ops in fn.insns:
        if (mn in ('b', 'cbz', 'cbnz', 'tbz', 'tbnz') or mn.startswith('b.')) and addr not in fn.relocs:
            for t in re.findall(r'0x([0-9a-f]+) <', ops):
                targets.add(int(t, 16))
    entry = {r: ('arg', i if r != 'x8' else 8) for i, r in enumerate(ARG_REGS)}
    regs = dict(entry)
    slots = {}  # frame slot -> provenance tuple of the value stored there

    def prov(r):
        r = reg64(r)
        if r in FRAME_BASES:
            return ('frame',)
        if r in ('xzr', 'wzr'):
            return ('const',)
        return regs.get(r)

    def classify(p):
        if p is None:
            return 'unknown'
        return {'frame': 'frame', 'const': 'frame', 'owned': 'value'}.get(p[0], 'nonframe')

    def storage(p, kind, addr, detail):
        """An ownership operation targets the address with provenance p."""
        c = classify(p)
        if c in ('frame', 'value'):
            return
        if p and p[0] == 'arg':
            storage_args.add(p[1])
        findings.append(('unknown' if c == 'unknown' else kind, addr, detail + f' ({c})'))

    for addr, mn, ops in fn.insns:
        if addr in targets:
            regs = {}
        args = [a.strip() for a in re.split(r',(?![^\[]*\])', ops)] if ops else []
        reloc = fn.relocs.get(addr)
        callee = None
        if mn == 'bl' and reloc:
            callee = reloc[1]
        elif mn == 'b' and reloc and reloc[0] == 'ARM64_RELOC_BRANCH26':
            callee = reloc[1]  # tail call
        elif mn.startswith('blr') and args and (prov(args[0]) or ('',))[0] == 'fn':
            callee = prov(args[0])[1]  # arm64e -O0 calls some runtime functions through a GOT-loaded pointer
        if callee:
            callee = callee[1:] if callee.startswith('_') else callee
            # Misses a created object returned through a local: ARC re-retains it first, as it does a parameter.
            if callee == 'objc_autoreleaseReturnValue' and prov('x0') == ('owned', 'created'):
                findings.append(('return', addr, f'{callee} of a +1 result'))
            if callee in ADDRESS_OPS:
                # ARC destroys a strong member with objc_storeStrong(&m, nil), and a weak one with objc_destroyWeak.
                destroys = callee == 'objc_destroyWeak' or (callee == 'objc_storeStrong' and prov('x1') == ('const',))
                for r in ADDRESS_OPS[callee]:
                    storage(prov(r), 'destroy' if destroys else 'runtime', addr, f'{callee}({r})')
            elif not exempt_callee('_' + callee):
                used = summaries.get(callee)
                if used is None and OWNERSHIP_IN_NAME.search(callee):
                    if any(classify(prov(r)) == 'nonframe' for r in ARG_REGS[:4]):
                        findings.append(('unknown', addr, f'{callee} (not defined in this object)'))
                for j in sorted(used or ()):
                    r = ARG_REGS[j] if j < 8 else 'x8'
                    p = prov(r)
                    if OWNERSHIP_IN_NAME.search(callee):
                        storage(p, 'sigil', addr, f'{callee}({r})')
                    elif p and p[0] == 'arg':
                        storage_args.add(p[1])  # a shared-name callee is examined on its own
            regs = {}
            if CREATE.match(callee):
                regs['x0'] = ('owned', 'created')
            elif RETAIN.match(callee) and not NOT_RETAIN.match(callee):
                regs['x0'] = ('owned', 'retained')
            continue
        if mn in ('bl', 'blr', 'blraa', 'blraaz', 'blrab', 'blrabz'):
            regs = {}
            continue
        if mn in ('str', 'stur', 'stp', 'stlr', 'strb', 'strh', 'sturb', 'sturh'):
            m = MEM.search(ops)
            if not m:
                continue
            base, imm = m.group(1), m.group(2)
            data = args[:2] if mn == 'stp' else args[:1]
            bp = prov(base)
            for i, d in enumerate(data):
                dp = prov(d)
                if classify(bp) == 'frame':
                    slot = offset_key(base, imm)
                    if mn == 'stp':
                        slot = (slot[0], slot[1] + 8 * i)
                    slots[slot] = dp if dp and dp[0] in ('frame', 'owned', 'arg') else ('other',)
                elif dp and dp[0] == 'owned':
                    storage(bp, 'store', addr, f'{mn} owned {reg64(d)} -> [{base}]')
            continue
        if not args:
            continue
        dst = reg64(args[0])
        if not re.fullmatch(r'x\d+', dst):
            continue
        if mn in ('ldr', 'ldur', 'ldar', 'ldapr'):
            m = MEM.search(ops)
            if m and m.group(1) in FRAME_BASES:
                sp = slots.get(offset_key(m.group(1), m.group(2)))
                regs[dst] = sp if sp and sp[0] in ('frame', 'owned', 'arg') else ('loaded',)
            elif m and prov(m.group(1)) == ('global',) and reloc and 'GOT' in reloc[0]:
                regs[dst] = ('fn', reloc[1])  # address of a symbol: a callee if called, a global if dereferenced
            else:
                regs[dst] = ('loaded',)
        elif mn == 'ldp':
            regs[dst] = ('loaded',)
            if len(args) > 1 and re.fullmatch(r'[xw]\d+', args[1]):
                regs[reg64(args[1])] = ('loaded',)
        elif mn == 'adrp':
            regs[dst] = ('global',)
        elif mn in ('add', 'sub') and len(args) >= 2:
            p = prov(args[1])
            regs[dst] = (p if p and p[0] in ('frame', 'global', 'loaded', 'arg') else None)
        elif mn == 'mov' and len(args) >= 2:
            src = args[1]
            regs[dst] = ('const',) if src.startswith('#') else prov(src)
        else:
            regs[dst] = None
    return findings, storage_args


def exempt_callee(mangled):
    return bool(EXEMPT_MANGLED.match(mangled))


_demangled = {}


def demangle_many(names):
    todo = [n for n in names if n not in _demangled]
    if todo:
        out = subprocess.run(['xcrun', 'c++filt', '-_'], input='\n'.join(todo), capture_output=True,
                             text=True).stdout.splitlines()
        _demangled.update(zip(todo, out))


def weak_functions(obj):
    out = subprocess.run(['nm', '-m', obj], capture_output=True, text=True).stdout
    return {line.split()[-1] for line in out.splitlines() if '(__TEXT,__text)' in line and ' weak ' in line}


def check_object(obj):
    weak = weak_functions(obj)
    fns = disassemble(obj)
    summaries = {(n[1:] if n.startswith('_') else n): set() for n in fns}
    for _ in range(12):  # storage_args only grow, so this reaches a fixed point
        changed = False
        for name, fn in fns.items():
            _, used = analyze(fn, summaries)
            key = name[1:] if name.startswith('_') else name
            if used != summaries[key]:
                summaries[key] = used
                changed = True
        if not changed:
            break
    results = {}
    for name in weak:
        if OWNERSHIP_IN_NAME.search(name) or BLOCK_HELPER.match(name) or SWIFT_NATIVE.match(name) or name not in fns:
            continue
        if EXEMPT_MANGLED.match(name):
            continue
        f, _ = analyze(fns[name], summaries)
        if f:
            decl = declaration_file(obj, fns[name].addr)
            if decl and decl.endswith(SOURCE_SUFFIXES):
                f = [('source-file', a, d) for _, a, d in f]
            elif decl and header_requires_arc(decl):
                f = [('arc-only-header', a, d) for _, a, d in f]
            results[name] = {'findings': f, 'decl': decl}
    return results


SOURCE_SUFFIXES = ('.mm', '.m', '.cpp', '.cc', '.c', '.swift')
ARC_GUARD = re.compile(r'^[ \t]*#[ \t]*if[ \t]+![ \t]*__has_feature[ \t]*\([ \t]*objc_arc[ \t]*\)[ \t]*\n'
                       r'[ \t]*#[ \t]*error\b', re.M)
_requires_arc = {}


def header_requires_arc(path):
    """Whether the header has the standard guard that makes it an error to include without ARC."""
    if path not in _requires_arc:
        try:
            _requires_arc[path] = bool(ARC_GUARD.search(open(path, errors='replace').read()))
        except OSError:
            _requires_arc[path] = False
    return _requires_arc[path]


def declaration_file(obj, addr):
    """The file that declares the function at addr, from debug info. A class declared in a source file
    can only be defined by that one file, so its weak functions can't collide."""
    out = subprocess.run(['xcrun', 'llvm-dwarfdump', f'--lookup=0x{addr:x}', obj], capture_output=True, text=True).stdout
    sub = out[out.find('DW_TAG_subprogram'):] if 'DW_TAG_subprogram' in out else ''
    for _ in range(3):  # a definition can point to its declaration (inside the class) or abstract origin
        m = re.search(r'DW_AT_decl_file\s+\("([^"]+)"\)', sub)
        if m:
            return m.group(1)
        ref = re.search(r'DW_AT_(?:specification|abstract_origin)\s+\((0x[0-9a-f]+)', sub)
        if not ref:
            return None
        sub = subprocess.run(['xcrun', 'llvm-dwarfdump', f'--debug-info={ref.group(1)}', obj], capture_output=True,
                             text=True).stdout
    return None


CLANG_OPT = re.compile(r'^-O(\d|s|z|g|fast)?$')
SWIFT_OPT = ('-Onone', '-O', '-Osize', '-Ounchecked')


def optimization_level(cmd, swift):
    """The optimization level a compile command uses: the last such flag wins."""
    tokens = cmd.split()
    if swift:
        levels = [t for t in tokens if t in SWIFT_OPT]
        return levels[-1] if levels else '-Onone'
    levels = [t for t in tokens if CLANG_OPT.match(t)]
    return levels[-1] if levels else '-O0'


def arc_objects(build):
    """ARC and Swift object files of the build, from compile_commands.json, mapped to the optimization
    level each was compiled with. Swift's Clang importer compiles imported headers with ARC, so Swift
    objects count as ARC."""
    objs = {}
    seen_maps = set()
    database = build if build.endswith('.json') else os.path.join(build, 'compile_commands.json')
    if not os.path.isfile(database):
        sys.exit(f'error: {database} not found; pass a build directory containing compile_commands.json, or the file')
    for e in json.load(open(database)):
        cmd = e.get('command') or ' '.join(e.get('arguments', []))
        f = e['file']
        if f.endswith('.swift'):
            m = re.search(r'(?:^|\s)-output-file-map\s+(\S+)', cmd)
            if not m:
                continue
            path = os.path.join(e['directory'], m.group(1))
            if path in seen_maps or not os.path.exists(path):
                continue
            seen_maps.add(path)
            level = optimization_level(cmd, swift=True)
            for entry in json.load(open(path)).values():
                if entry.get('object'):
                    objs[os.path.normpath(os.path.join(e['directory'], entry['object']))] = level
        elif f.endswith(('.m', '.mm')) and '-emit-pch' not in cmd:
            flags = [t for t in cmd.split() if t in ('-fobjc-arc', '-fno-objc-arc')]
            m = re.search(r'(?:^|\s)-o\s+(\S+)', cmd)
            if flags and flags[-1] == '-fobjc-arc' and m:
                objs[os.path.normpath(os.path.join(e['directory'], m.group(1)))] = optimization_level(cmd, swift=False)
    return objs


def main():
    ap = argparse.ArgumentParser(description='Check a -O0 build for ARC / non-ARC ODR violations in inline functions.')
    ap.add_argument('objects', nargs='*', help='object files to check')
    ap.add_argument('--build', help='build directory containing compile_commands.json, or the file itself')
    ap.add_argument('--out', help='write all findings as JSON')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 8)
    ap.add_argument('--exclude', action='append', default=['/ThirdParty/libwebrtc/'],
                    help='skip objects whose path contains this string (default: libwebrtc)')
    a = ap.parse_args()
    if not a.objects and not a.build:
        ap.error('pass --build BUILD_DIR or object files')
    objs = list(a.objects)
    levels = arc_objects(a.build) if a.build else {}
    objs += sorted(levels)
    objs = [o for o in objs if not any(x in o for x in a.exclude)]
    missing = [o for o in objs if not os.path.exists(o)]
    objs = [o for o in objs if os.path.exists(o)]
    if missing:
        print(f'{len(missing)} object files in compile_commands.json have not been built; skipping them')
    optimized = [o for o in objs if levels.get(o, '-O0') not in ('-O0', '-Onone')]
    if optimized:
        print(f'error: {len(optimized)} of {len(objs)} objects were compiled with optimization; this check is only '
              f'valid for -O0 (Debug) builds. For example:')
        for o in optimized[:5]:
            print(f'    {levels[o]}  {o}')
        sys.exit(2)
    report = {}
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        try:
            for obj, res in zip(objs, ex.map(check_object, objs)):
                for name, r in res.items():
                    report.setdefault(name, {'objects': [], **r})['objects'].append(obj)
        except BaseException:
            ex.shutdown(cancel_futures=True)  # otherwise the error waits for every queued object to finish
            raise
    demangle_many(sorted(report))
    print(f'{len(objs)} objects; {len(report)} functions flagged')
    root = source_root(a.build)

    def entries(names):
        rows = []
        for n in names:
            kinds = sorted({TAGS.get(k, k) for k, _, _ in report[n]['findings']})
            rows.append((display_path(report[n]['decl'], root), _demangled.get(n, n), ','.join(kinds)))
        width = max((len(k) for _, _, k in rows), default=0)
        for path, pretty, kinds in sorted(rows):
            print(f'        {kinds:{width}}  {path}: {pretty}')

    def plural(n):
        return f'{n} flagged function' + ('' if n == 1 else 's')

    actionable = [n for n, v in report.items() if any(k not in INFORMATIONAL for k, _, _ in v['findings'])]
    allowed = [n for n in report if n not in actionable]
    if allowed:
        print(f'    => ALLOWED: {plural(len(allowed))}')
        entries(allowed)
    if actionable:
        print(f'    => NOT ALLOWED: {plural(len(actionable))} doing ARC ownership work on storage outside the stack frame')
        entries(actionable)
    if a.out:
        json.dump({_demangled.get(n, n): {'mangled': n, **v} for n, v in report.items()}, open(a.out, 'w'), indent=1)
    print(f'{len(actionable)} actionable findings')
    sys.exit(1 if actionable else 0)


def source_root(build):
    """The checkout containing the build directory or compile_commands.json (the parent of WebKitBuild), for
    shorter paths."""
    d = os.path.abspath(build) if build else None
    while d and d != os.path.dirname(d):
        if os.path.isdir(os.path.join(d, 'WebKitBuild')):
            return d
        d = os.path.dirname(d)
    return None


def display_path(path, root):
    if not path:
        return '(declaration file unknown)'
    if root and path.startswith(root + os.sep):
        return os.path.relpath(path, root)
    return path


INFORMATIONAL = {'source-file', 'arc-only-header'}
TAGS = {
    'source-file': 'not-in-header',
    'arc-only-header': 'arc-only-header',
    'runtime': 'assigns-objc-member',
    'destroy': 'destroys-objc-member',
    'store': 'initializes-objc-member',
    'sigil': 'passes-member-to-arc-only-code',
    'return': 'returns-autoreleased-new-object',
    'unknown': 'unclassified',
}


if __name__ == '__main__':
    main()
