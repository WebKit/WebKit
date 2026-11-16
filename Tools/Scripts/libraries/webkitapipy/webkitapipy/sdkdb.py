# Copyright (C) 2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import annotations

import json
import os
import sqlite3
from enum import Enum
from fnmatch import fnmatch
from typing import Any, Callable, Iterable, Mapping, NamedTuple, Optional, Union
from pathlib import Path

from .macho import APIReport, objc_fully_qualified_method
from .tbd import TBD
from .allow import AllowList
from .swift_mangle import mangle_partial

# Increment this number to force clients to rebuild from scratch, to
# accomodate schema changes or fix caching bugs.
VERSION = 10


class DeclarationKind(Enum):
    SYMBOL = 1
    OBJC_CLS = 2
    OBJC_SEL = 3

    def __str__(self):
        if self is self.SYMBOL:
            return 'symbol'
        elif self is self.OBJC_CLS:
            return 'class'
        else:
            return 'selector'

    def to_sql(self):
        return self.name

    @classmethod
    def from_sql(cls, value: bytes) -> DeclarationKind:
        return cls[value.decode()]


sqlite3.register_adapter(DeclarationKind, DeclarationKind.to_sql)
sqlite3.register_converter("DeclarationKind", DeclarationKind.from_sql)

PLATFORM = '__PLATFORM'
OS_VERSION = '__OS_VERSION'
SDK_VERSION = '__SDK_VERSION'


def apply_operator_sql(op: str, value: str, operand: str) -> str:
    return (
        f"(CASE ({op})"
        f" WHEN '==' THEN ({value}) IS ({operand})"
        f" WHEN '!=' THEN ({value}) IS NOT ({operand})"
        f" WHEN '<'  THEN ({value}) < ({operand})"
        f" WHEN '<=' THEN ({value}) <= ({operand})"
        f" WHEN '>'  THEN ({value}) > ({operand})"
        f" WHEN '>=' THEN ({value}) >= ({operand})"
        f" END)"
    )


def semver_to_int(semver: str) -> int:
    version = tuple(map(int, semver.split('.')))
    if len(version) > 3 or any(x > 99 for x in version):
        raise ValueError(f'Semantic version "{semver}" must be no more than '
                         '3 components, with each component below 99')
    elif len(version) == 3:
        return version[0] * 10000 + version[1] * 100 + version[2]
    elif len(version) == 2:
        return version[0] * 10000 + version[1] * 100
    else:
        return version[0] * 10000


class MissingName(NamedTuple):
    name: str
    file: Path
    arch: str
    kind: DeclarationKind


class UnusedAllowedName(NamedTuple):
    name: str
    file: Path
    kind: DeclarationKind


class UnnecessaryAllowedName(NamedTuple):
    name: str
    file: Path
    kind: DeclarationKind
    exported_in: Path


Diagnostic = Union[MissingName, UnusedAllowedName, UnnecessaryAllowedName]
ConditionVariable = Union[bool, str, int, float]

SYMBOL = DeclarationKind.SYMBOL
OBJC_CLS = DeclarationKind.OBJC_CLS
OBJC_SEL = DeclarationKind.OBJC_SEL

_OBJC_CLASS_ = '_OBJC_CLASS_$_'
_OBJC_METACLASS_ = '_OBJC_METACLASS_$_'


class SDKDB:
    """
    A sqlite-backed cache of API availability data. Composed from a variety of
    sources:

    - Symbols and Objective-C runtime metadata from partial SDKDB (.sdkdb)
      records produced by TAPI
    - Symbols from text-based library stubs (.tbd).
    - Symbols from Mach-O binaries.

    The cache keeps track of the file name which each record comes from, and
    the modification time of each input file. The add_* methods take a path
    and short circuit if that file is already in the cache with the same mtime.
    When an input file changes, all the records associated with it are deleted
    and it is re-added.

    To manage concurrent access to a cache, enter the sdkdb as a context
    manager before using the add_* methods.
    """

    def __init__(self, db_file: Path):
        user_version = None
        while user_version != VERSION:
            self.con = sqlite3.connect(db_file, isolation_level='IMMEDIATE',
                                       detect_types=sqlite3.PARSE_DECLTYPES)
            self.con.execute('PRAGMA busy_timeout = 300000')
            self.con.execute('PRAGMA foreign_keys = ON')
            user_version, = self.con.execute('PRAGMA user_version').fetchone()
            if user_version == 0:
                try:
                    self._initialize_db()
                except sqlite3.OperationalError:
                    # Delete the database on initialization errors, in case
                    # rebuilding fixes it.
                    self.con.close()
                    db_file.unlink()
                    raise
            elif user_version != VERSION:
                print(f'Rebuilding {db_file} due to version change')
                self.con.close()
                db_file.unlink()
        self._initialize_temporary_schema()

    def _initialize_db(self):
        cur = self.con.cursor()
        # Python's sqlite3 module does not start a transaction for DDL
        # statements. Explicitly BEGIN to prevent concurrent processes from
        # trying to initialize the database simultaneously.
        cur.execute('BEGIN IMMEDIATE TRANSACTION')
        cur.execute('PRAGMA user_version')
        if cur.fetchone() == (VERSION,):
            # The database was initialized while we were waiting.
            return
        cur.execute('CREATE TABLE input_file(path PRIMARY KEY, hash)')
        cur.execute('CREATE TABLE exports('
                    '   name TEXT, class_name, kind DeclarationKind, '
                    '   input_file REFERENCES input_file(path) '
                    '              ON DELETE CASCADE)')
        cur.execute('CREATE INDEX export_names ON exports (name, kind)')
        cur.execute('CREATE TABLE allow('
                    '   name TEXT, is_prefix, class_name, allow_unused, '
                    '   kind DeclarationKind, '
                    '   cond_id, input_file REFERENCES input_file(path) '
                    '                       ON DELETE CASCADE)')
        cur.execute('CREATE INDEX allow_names ON allow (name, kind, is_prefix)')
        cur.execute('CREATE TABLE condition_chain(name, '
                    "   op CHECK (op IN ('==', '!=', '<', '<=', '>', '>=')), "
                    '   operand, nextid, '
                    '   input_file REFERENCES input_file(path) '
                    '              ON DELETE CASCADE)')
        cur.execute(f'PRAGMA user_version = {VERSION}')
        self.con.commit()

    def _initialize_temporary_schema(self):
        cur = self.con.cursor()
        cur.execute('CREATE TEMPORARY TABLE window(input_file)')
        cur.execute('CREATE INDEX selected_files ON window(input_file)')
        cur.execute('CREATE TEMPORARY TABLE imports(name TEXT, '
                    '   kind DeclarationKind, input_file, arch)')
        cur.execute('CREATE INDEX import_names ON imports(name, kind)')
        cur.execute('CREATE TEMPORARY TABLE condition(name UNIQUE, value)')
        cur.execute('CREATE TEMPORARY TABLE active_cond(nextid, name TEXT)')
        self.con.commit()

    def __del__(self):
        if not hasattr(self, 'con'):
            return
        # May fail if the connection is closed (due to failure to initialize)
        # or not writable (due to the file being deleted on disk).
        try:
            self.con.execute('PRAGMA optimize')
        except (sqlite3.ProgrammingError, sqlite3.OperationalError):
            pass

    def __enter__(self):
        # Python's sqlite3 module will issue a BEGIN before the first data
        # modification. No need to explicitly start a transaction earlier.
        pass

    def __exit__(self, exc_type, exc_value, traceback):
        if exc_type:
            self.con.rollback()
        else:
            self.con.commit()

    def _cache_hit_preparing_to_insert(self, file: Path, hash_: int) -> bool:
        # Normalize the path so that different projects can refer to the main
        # SDKDB directory through relative paths. resolve() normalizes
        # symlinks, which can confuse XCBuild's dependency tracking, but the
        # input paths in the depfile are written, unresolved, from
        # program.main(). So this path is purely an internal representation.
        path = str(file.resolve())
        cur = self.con.cursor()
        cur.execute('SELECT hash from input_file where path = ?', (path,))
        if cur.fetchone() == (hash_,):
            cached = True
        else:
            # To support files *removing* declarations from the sdkdb cache,
            # it's important to execute DELETE and INSERT separately, instead
            # of using an an upsert. The deletion cascades to any declarations
            # tracked from the file.
            cur.execute('DELETE FROM input_file WHERE path = ?', (path,))
            cur.execute('INSERT INTO input_file VALUES (?, ?)', (path, hash_))
            cached = False
        cur.execute('INSERT INTO window VALUES (?)', (path,))
        return cached

    def add_partial_sdkdb(self, sdkdb_file: Path, *, spi=False, abi=False,
                          ) -> bool:
        fd = open(sdkdb_file)
        sdkdb_hash = os.fstat(fd.fileno()).st_mtime_ns

        if self._cache_hit_preparing_to_insert(sdkdb_file,
                                               sdkdb_hash):
            return False
        doc = json.load(fd)
        self._add_partial_sdkdb(doc, sdkdb_file, spi, abi)
        return True

    def _add_partial_sdkdb(self, doc: dict[str, Any], sdkdb_file: Path,
                           spi: bool, abi: bool):
        criteria = [
            ('PublicSDKContentRoot', lambda _: True),
            ('SDKContentRoot',
             lambda x: spi or x.get('access') == 'public'),
        ]
        if abi:
            criteria.append(('RuntimeRoot', lambda _: True))
        for key, pred in criteria:
            root = doc.get(key, ())
            for ent in root:
                for category in ent.get('categories', []):
                    class_name = f'{category["interface"]}'\
                                 f'({category["name"]})'
                    self._add_objc_interface(category, class_name,
                                             sdkdb_file, pred)
                for symbol in ent.get('globals', []):
                    if pred(symbol):
                        self._add_symbol(symbol['name'], sdkdb_file)
                for iface in ent.get('interfaces', []):
                    if pred(iface):
                        self._add_objc_interface(iface, iface['name'],
                                                 sdkdb_file, pred)
                        self._add_objc_class(iface['name'], sdkdb_file)
                for proto in ent.get('protocols', []):
                    if pred(proto):
                        self._add_objc_interface(proto, proto['name'],
                                                 sdkdb_file, pred)
        return True

    def add_binary(self, binary: Path, arch: str, *,
                   for_auditing: bool = False) -> bool:
        # When only adding a binary to check exports, avoid generating the
        # report until we know it missed the cache.
        if for_auditing:
            report = APIReport.from_binary(binary, arch=arch,
                                           exports_only=False)
            self._add_imports(report)
        stat_hash = binary.stat().st_mtime_ns
        if self._cache_hit_preparing_to_insert(binary, stat_hash):
            return False
        if not for_auditing:
            report = APIReport.from_binary(binary, arch=arch,
                                           exports_only=True)
        self._add_api_report(report, binary)
        return True

    class InsertionKind(Enum):
        EXPORTS = 1
        ALLOW = 2
        ALLOW_PREFIX = 3

        @property
        def statement(self) -> str:
            if self == self.EXPORTS:
                return ('INSERT INTO exports VALUES (:name, :class_name, '
                        '                            :kind, :file)')
            elif self == self.ALLOW:
                return ('INSERT INTO allow VALUES (:name, 0, :class_name, '
                        '                           :allow_unused, :kind, '
                        '                           :cond, :file)')
            elif self == self.ALLOW_PREFIX:
                return ('INSERT INTO allow VALUES (:name, 1, :class_name, '
                        '                          :allow_unused, :kind, '
                        '                          :cond, :file)')
            else:
                raise RuntimeError('unreachable')

    def _add_api_report(self, report: APIReport, binary: Path,
                        dest=InsertionKind.EXPORTS):
        for sel in report.methods:
            self._add_objc_selector(sel.name, sel.class_, binary, dest=dest)
        for symbol in report.exports:
            m = objc_fully_qualified_method.match(symbol)
            if m:
                self._add_objc_selector(m.group('selector'),
                                        m.group('class'), binary, dest=dest)
            elif symbol.startswith('_OBJC_CLASS_$_'):
                self._add_objc_class(symbol.removeprefix('_OBJC_CLASS_$_'),
                                     binary, dest=dest)
            else:
                self._add_symbol(symbol, binary, dest=dest)

    def add_tbd(self, tbd_file: Path,
                only_including: Optional[Iterable[str]]) -> bool:
        fd = open(tbd_file)
        stat_hash = os.fstat(fd.fileno()).st_mtime_ns

        if self._cache_hit_preparing_to_insert(tbd_file, stat_hash):
            return False
        for tbd in TBD.from_file(tbd_file):
            if only_including and all(not fnmatch(tbd.install_name, pattern)
                                      for pattern in only_including):
                continue
            for export_list in tbd.exports + tbd.reexports:
                for symbol in export_list.symbols:
                    self._add_symbol(symbol, tbd_file)
                for symbol in export_list.weak_symbols:
                    self._add_symbol(symbol, tbd_file)
                for class_ in export_list.objc_classes:
                    self._add_objc_class(class_, tbd_file)
        return True

    def add_allowlist(self, allowlist: Path) -> bool:
        stat_hash = allowlist.stat().st_mtime_ns
        if self._cache_hit_preparing_to_insert(allowlist, stat_hash):
            return False
        config = AllowList.from_file(allowlist)
        self._add_allowlist(config, allowlist)
        return True

    def _add_allowlist(self, config: AllowList, allowlist: Path):
        for entry in config.allowed_spi:
            cond_id = None
            cur = self.con.cursor()
            if entry.requires:
                # Convert a requirements list like ["A", "B", "!C"] into a
                # graph data structure e.g. (A) -> (B) -> (!C). The head node
                # is associated with each allowed declaration. The audit() query
                # cross-references condition chains with active conditions
                # added by add_defines().
                #
                # FIXME: No effort is made to reuse nodes in the graph between
                # allowlist entries, so we store more than we have to.
                # (https://bugs.webkit.org/show_bug.cgi?id=295819)
                for name in reversed(entry.requires):
                    cur.execute('INSERT INTO condition_chain VALUES (?,?,?,?,?)',
                                (name.removeprefix('!'),
                                 '!=' if name.startswith('!') else '==', 1,
                                 cond_id, str(allowlist.resolve())))
                    cond_id = cur.lastrowid
            for req in entry.requires_os:
                cur.execute('INSERT INTO condition_chain VALUES (?,?,?,?,?)',
                            (f'{OS_VERSION}_{req.platform}', req.operator,
                             semver_to_int(req.version), cond_id,
                             str(allowlist.resolve())))
                cond_id = cur.lastrowid
            for req in entry.requires_sdk:
                cur.execute('INSERT INTO condition_chain VALUES (?,?,?,?,?)',
                            (f'{SDK_VERSION}_{req.platform}', req.operator,
                             semver_to_int(req.version), cond_id,
                             str(allowlist.resolve())))
                cond_id = cur.lastrowid
            for symbol in entry.symbols:
                self._add_symbol(symbol, allowlist,
                                 dest=self.InsertionKind.ALLOW,
                                 cond_id=cond_id,
                                 allow_unused=entry.allow_unused)
            for class_ in entry.classes:
                self._add_objc_class(class_, allowlist,
                                     dest=self.InsertionKind.ALLOW,
                                     cond_id=cond_id,
                                     allow_unused=entry.allow_unused)
            for sel in entry.selectors:
                self._add_objc_selector(sel.name, sel.class_, allowlist,
                                        dest=self.InsertionKind.ALLOW,
                                        cond_id=cond_id,
                                        allow_unused=entry.allow_unused)
            for decl in entry.swift_decls:
                mangled = mangle_partial(decl.name, type_kinds=decl.type_kinds,
                                         extension_module=decl.extension,
                                         extension_base_depth=decl.extension_base_depth)
                assert all(c != '*' and c != '?' for c in mangled), \
                    'Mangled Swift symbol "{mangled}" contains glob characters'
                self._add_symbol(mangled, allowlist,
                                 dest=self.InsertionKind.ALLOW_PREFIX,
                                 cond_id=cond_id,
                                 allow_unused=entry.allow_unused)

    def add_conditions(self, conditions: Mapping[str, ConditionVariable]):
        cur = self.con.cursor()
        cur.executemany('INSERT INTO condition VALUES (?,?) '
                        'ON CONFLICT DO UPDATE SET value = (?)',
                        ((k, v, v) for k, v in conditions.items()))

    def _add_imports(self, report: APIReport):
        cur = self.con.cursor()
        path = str(report.file.resolve())
        arch = report.arch

        # FIXME: It doesn't really make sense to add these per-report; they should be set once per run.
        self.add_conditions({
            f'{OS_VERSION}_{report.platform}': semver_to_int(report.min_os),
            f'{SDK_VERSION}_{report.platform}': semver_to_int(report.sdk)
        })

        # Don't use _cache_hit_preparing_to_insert to update the window. The
        # imports table is not persisted, and we don't want to prevent a
        # different invocation that reads exports from this binary from
        # inserting to the exports table.
        cur.execute('INSERT INTO window VALUES (?)', (path,))
        cur.executemany('INSERT INTO imports VALUES (?, ?, ?, ?)',
                        ((sym.removeprefix(_OBJC_CLASS_), OBJC_CLS,
                          path, arch) if sym.startswith(_OBJC_CLASS_) else
                         (sym.removeprefix(_OBJC_METACLASS_), OBJC_CLS,
                          path, arch) if sym.startswith(_OBJC_METACLASS_) else
                         (sym, SYMBOL, path, arch)
                         for sym in report.imports))
        cur.executemany('INSERT INTO imports VALUES (?, ?, ?, ?)',
                        ((sel, OBJC_SEL, path, report.arch)
                         for sel in report.selrefs))

    def backup_temp_data(self, file: Path) -> None:
        backup = sqlite3.connect(file)
        self.con.commit()
        self.con.backup(backup, pages=1, name='temp')
        backup.close()

    def _materialize_active_conditions(self, cur) -> None:
        """Populate a temp table by traversing the condition_chain graph to
        find allowlist entries whose conditions are all satisfied."""
        cur.execute('DELETE FROM active_cond')
        cur.execute('INSERT INTO active_cond '
                    'WITH RECURSIVE ac AS ('
                    '   SELECT cc.rowid AS nextid, name '
                    '   FROM condition_chain AS cc NATURAL LEFT JOIN condition '
                    '   WHERE cc.nextid IS NULL AND '
                    f'        {apply_operator_sql("cc.op", "condition.value", "cc.operand")}'
                    '   UNION ALL '
                    '   SELECT cc.rowid AS nextid, cc.name '
                    '   FROM condition_chain AS cc JOIN ac USING (nextid) '
                    '   NATURAL LEFT JOIN condition '
                    f'  WHERE {apply_operator_sql("cc.op", "condition.value", "cc.operand")}'
                    ') SELECT * FROM ac')

    def _query_missing_imports(self, cur) -> list:
        """Find imported names that have no matching export in any loaded file."""
        cur.execute('SELECT i.arch, i.kind, i.input_file, i.name, '
                    '       sum(e.name IS NOT NULL AND '
                    '           ew.input_file IS NOT NULL) as export_found '
                    'FROM imports AS i '
                    'LEFT JOIN exports AS e USING (name, kind) '
                    'FULL JOIN window AS ew ON ew.input_file = e.input_file '
                    'GROUP BY i.kind, i.name, i.input_file '
                    'HAVING i.name IS NOT NULL AND export_found = 0 '
                    'ORDER BY i.input_file, i.kind, i.name')
        return cur.fetchall()

    # Note on prefix matching: The below queries use range lookups instead of
    # LIKE or GLOB to match names in allowlist entries. Plain LIKE and GLOB
    # cannot be optimized because the query planner doesn't know whether the
    # pattern string is a prefix match or not, so the LIKE optimization
    # <https://sqlite.org/optoverview.html#the_like_optimization> does not
    # apply.
    #
    # The range lookup (q >= p AND q < p||X'FF') is equivalent to what the LIKE
    # optimization would expand to. The upper bound of the range works because
    # 0xFF is larger than any starting UTF-8 byte, so it sorts after `name` but
    # before the next possible UTF-8 string.

    def _query_allowed_imports(self, cur) -> list:
        """Find imported names that match a loaded allowlist entry."""
        cur.execute('SELECT i.kind, i.name, '
                    '       a.kind, group_concat(aw.input_file), a.name, a.class_name, '
                    '           max(a.is_prefix), min(a.allow_unused), '
                    '       sum(a.name IS NOT NULL AND '
                    '           a.cond_id IS c.nextid AND '
                    '           aw.input_file IS NOT NULL) as allow_found '
                    'FROM allow AS a '
                    'LEFT JOIN imports AS i '
                    '     ON (i.name = a.name '
                    '         OR (a.is_prefix AND i.name >= a.name '
                    '             AND i.name < a.name||X\'FF\')) '
                    '        AND i.kind = a.kind '
                    'FULL JOIN window AS aw ON aw.input_file = a.input_file '
                    'LEFT JOIN active_cond AS c ON a.cond_id = c.nextid '
                    'GROUP BY i.kind, a.kind, i.name, a.name, i.input_file '
                    'HAVING allow_found > 0 '
                    'ORDER BY i.input_file, i.kind, a.kind, i.name, a.name')
        return cur.fetchall()

    def _find_export_sources(self, cur, allowed_name: str, allowed_kind: DeclarationKind,
                             allowed_class: Optional[str], is_prefix: bool) -> list:
        """Look up which loaded files export a given name."""
        params = {'name': allowed_name, 'kind': allowed_kind, 'class': allowed_class}
        where_clause = ('e.name >= :name AND e.name < :name||X\'FF\''
                        if is_prefix else 'e.name = :name')
        cur.execute('SELECT DISTINCT e.input_file FROM exports AS e '
                    'JOIN window AS ew ON ew.input_file = e.input_file '
                    f'WHERE {where_clause} AND e.kind = :kind '
                    # For ObjC selectors, ignore mismatching classes if the
                    # allowlist entry has a class.
                    'AND (e.class_name = :class OR :class IS NULL) '
                    'ORDER BY e.input_file ', params)
        return cur.fetchall()

    def audit(self, debug_query_plan=False) -> Iterable[Diagnostic]:
        cur = self.con.cursor()

        self._materialize_active_conditions(cur)
        # Maps from a name-kind pair representing an imported declaration to
        # the file-arch slice that it appears in.
        missing = {}
        for arch, kind, path, name, _ in self._query_missing_imports(cur):
            missing[(name, kind)] = (arch, path)

        # Tracks when a prefix symbol for a swift declaration has been
        # encountered, to avoid reporting issues for the same entry multiple
        # times.
        seen_prefixes: set[str] = set()

        for (import_kind, import_name,
             allowed_kind, allowlist_paths, allowed_name, allowed_class,
             is_prefix, allow_unused, _) in self._query_allowed_imports(cur):
            display_name = allowed_name + ('*' if is_prefix else '')
            allowlist_files = sorted(set(allowlist_paths.split(',')))

            if import_name is None:
                # Not imported: allowlist entry can be cleaned up.
                if allow_unused:
                    continue
                for path in allowlist_files:
                    yield UnusedAllowedName(name=display_name,
                                            file=Path(path), kind=allowed_kind)
            else:
                if missing.pop((import_name, import_kind), None):
                    # Matches a missing declaration: SPI allowed.
                    continue
                # Imported, allowed, and present in the exports table:
                # allowlist entry can be cleaned up.
                if is_prefix:
                    if allowed_name in seen_prefixes:
                        continue
                    seen_prefixes.add(allowed_name)
                for export_file, in self._find_export_sources(
                        cur, allowed_name, allowed_kind,
                        allowed_class, is_prefix):
                    for path in allowlist_files:
                        yield UnnecessaryAllowedName(
                            name=display_name, file=Path(path),
                            kind=allowed_kind, exported_in=Path(export_file))

        # Any remaining missing entries are not covered by active allowlist
        # entries.
        for (name, kind), (arch, input_file) in missing.items():
            yield MissingName(name=name, file=Path(input_file),
                              arch=arch, kind=kind)

    def stats(self):
        cur = self.con.cursor()
        cur.execute('SELECT (SELECT COUNT(name) FROM exports WHERE kind = ?), '
                    '(SELECT COUNT(name) FROM exports WHERE kind = ?), '
                    '(SELECT COUNT(name) FROM exports WHERE kind = ?) ',
                    (SYMBOL, OBJC_CLS, OBJC_SEL))
        row = cur.fetchone()
        return row

    def _add_objc_interface(self, ent: dict, class_name: str, file: Path,
                            pred: Callable[[dict], bool],
                            dest=InsertionKind.EXPORTS,
                            cond_id: Optional[int] = None):
        for key in 'instanceMethods', 'classMethods':
            for method in ent.get(key, []):
                if pred(method):
                    self._add_objc_selector(method['name'], class_name, file,
                                            dest=dest)
        for prop in ent.get('properties', []):
            if pred(prop):
                self._add_objc_selector(prop['getter'], class_name, file,
                                        dest=dest)
                if 'readonly' not in prop.get('attr', {}):
                    self._add_objc_selector(prop['setter'], class_name, file,
                                            dest=dest)
        for ivar in ent.get('ivars', []):
            if pred(ivar):
                self._add_symbol(f'_OBJC_IVAR_$_{class_name}.{ivar["name"]}',
                                 file, dest=dest)

    def _add_symbol(self, name: str, file: Path,
                    dest=InsertionKind.EXPORTS,
                    allow_unused: Optional[bool] = None,
                    cond_id: Optional[int] = None):
        cur = self.con.cursor()
        params = dict(name=name, class_name=None, kind=SYMBOL,
                      file=str(file.resolve()), cond=cond_id,
                      allow_unused=allow_unused)
        cur.execute(dest.statement, params)

    def _add_objc_class(self, name: str, file: Path,
                        dest=InsertionKind.EXPORTS,
                        allow_unused: Optional[bool] = None,
                        cond_id: Optional[int] = None):
        cur = self.con.cursor()
        params = dict(name=name, class_name=None, kind=OBJC_CLS,
                      file=str(file.resolve()), cond=cond_id,
                      allow_unused=allow_unused)
        cur.execute(dest.statement, params)

    def _add_objc_selector(self, name: str, class_name: Optional[str],
                           file: Path, dest=InsertionKind.EXPORTS,
                           allow_unused: Optional[bool] = None,
                           cond_id: Optional[int] = None):
        cur = self.con.cursor()
        params = dict(name=name, class_name=class_name,
                      kind=OBJC_SEL, file=str(file.resolve()), cond=cond_id,
                      allow_unused=allow_unused)
        cur.execute(dest.statement, params)
