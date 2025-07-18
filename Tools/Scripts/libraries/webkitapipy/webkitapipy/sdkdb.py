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
from typing import Callable, Iterable, NamedTuple, Optional, Union
from pathlib import Path

from .macho import APIReport, objc_fully_qualified_method
from .tbd import TBD
from .allow import AllowList

# Increment this number to force clients to rebuild from scratch, to
# accomodate schema changes or fix caching bugs.
VERSION = 6


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
    def from_sql(cls, value: bytes):
        return cls[value.decode()]


sqlite3.register_adapter(DeclarationKind, DeclarationKind.to_sql)
sqlite3.register_converter("DeclarationKind", DeclarationKind.from_sql)


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
            self.con.execute('PRAGMA busy_timeout = 30000')
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
        cur.execute('CREATE TABLE exports(name, kind DeclarationKind, '
                    '   input_file REFERENCES input_file(path) '
                    '              ON DELETE CASCADE)')
        cur.execute('CREATE INDEX export_names ON exports (name, kind)')
        cur.execute('CREATE TABLE allow('
                    '   name, kind DeclarationKind, cond_id,'
                    '   input_file REFERENCES input_file(path) '
                    '              ON DELETE CASCADE)')
        cur.execute('CREATE INDEX allow_names ON allow (name, kind)')
        cur.execute('CREATE TABLE condition_chain(name, invert, nextid, '
                    '   input_file REFERENCES input_file(path) '
                    '              ON DELETE CASCADE)')
        cur.execute(f'PRAGMA user_version = {VERSION}')
        self.con.commit()

    def _initialize_temporary_schema(self):
        cur = self.con.cursor()
        cur.execute('CREATE TEMPORARY TABLE window(input_file)')
        cur.execute('CREATE INDEX selected_files ON window(input_file)')
        cur.execute('CREATE TEMPORARY TABLE imports(name, '
                    '   kind DeclarationKind, input_file, arch)')
        cur.execute('CREATE INDEX import_names ON imports(name, kind)')
        cur.execute('CREATE TEMPORARY TABLE condition(name UNIQUE)')
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

    def add_binary(self, binary: Path, arch: str) -> bool:
        stat_hash = binary.stat().st_mtime_ns
        if self._cache_hit_preparing_to_insert(binary, stat_hash):
            return False
        report = APIReport.from_binary(binary, arch=arch, exports_only=True)
        self._add_api_report(report, binary)
        return True

    class InsertionKind(Enum):
        EXPORTS = 1
        ALLOW = 2

        @property
        def statement(self) -> str:
            if self == self.EXPORTS:
                return f'INSERT INTO exports VALUES (:name, :kind, :file)'
            else:  # self.ALLOW
                return f'INSERT INTO allow VALUES (:name, :kind, :cond, :file)'

    def _add_api_report(self, report: APIReport, binary: Path,
                        dest=InsertionKind.EXPORTS):
        for selector in report.methods:
            self._add_objc_selector(selector, None, binary, dest=dest)
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
                cur = self.con.cursor()
                for req in reversed(entry.requires):
                    cur.execute('INSERT INTO condition_chain VALUES (?,?,?,?)',
                                (req.removeprefix('!'), req.startswith('!'),
                                 cond_id, str(allowlist.resolve())))
                    cond_id = cur.lastrowid
            for symbol in entry.symbols:
                self._add_symbol(symbol, allowlist,
                                 dest=self.InsertionKind.ALLOW,
                                 cond_id=cond_id)
            for class_ in entry.classes:
                self._add_objc_class(class_, allowlist,
                                     dest=self.InsertionKind.ALLOW,
                                     cond_id=cond_id)
            for selector in entry.selectors:
                self._add_objc_selector(selector, None, allowlist,
                                        dest=self.InsertionKind.ALLOW,
                                        cond_id=cond_id)

    def add_defines(self, defines: list[str]):
        cur = self.con.cursor()
        cur.executemany('INSERT INTO condition VALUES (?)',
                        ((d,) for d in defines))

    def add_for_auditing(self, report: APIReport):
        cur = self.con.cursor()
        path = str(report.file.resolve())
        arch = report.arch
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
        # Some ObjC selectors may be implemented by methods in the binary.
        # Since this binary's exports are not added to the cache, the query
        # won't weed these false positives out. Instead, remove them via the
        # `if` clause below.
        cur.executemany('INSERT INTO imports VALUES (?, ?, ?, ?)',
                        ((sel, OBJC_SEL, path, report.arch)
                         for sel in report.selrefs
                         if sel not in report.methods))

    def audit(self) -> Iterable[Diagnostic]:
        cur = self.con.cursor()
        # First compute the "-D" defines that are active by traversing the
        # `condition_chain` graph structure. Start with rows that have no
        # `nextid` edge, and keep only the ones whose names are active (or
        # inactive, if the condition is inverted). Repeat the process with rows
        # whose nextid is in the table, until no rows are added.
        cur.execute('WITH RECURSIVE active_cond AS ('
                    '   SELECT cc.rowid AS nextid, name '
                    '   FROM condition_chain AS cc NATURAL LEFT JOIN condition '
                    '   WHERE nextid IS NULL AND iif(cc.invert, '
                    '                                condition.name IS NULL, '
                    '                                condition.name IS NOT NULL) '
                    '   UNION ALL '
                    '   SELECT cc.rowid AS nextid, cc.name '
                    '   FROM condition_chain AS cc JOIN active_cond USING (nextid) '
                    '   NATURAL LEFT JOIN condition '
                    '   WHERE iif(cc.invert, '
                    '             condition.name IS NULL, '
                    '             condition.name IS NOT NULL)'
                    ') '
                    # Then cross-check imports and allowed declarations against
                    # exports.
                    'SELECT i.arch, i.kind, i.input_file, i.name, '
                    '       a.kind, group_concat(aw.input_file), a.name, '
                    '       ew.input_file, '
                    '       sum(e.name IS NOT NULL AND '
                    '           ew.input_file IS NOT NULL) as export_found, '
                    '       sum(a.name IS NOT NULL AND '
                    '           a.cond_id IS c.nextid AND '
                    '           aw.input_file IS NOT NULL) as allow_found '
                    'FROM imports AS i '
                    'LEFT JOIN exports AS e USING (name, kind) '
                    'FULL JOIN allow AS a USING (name, kind) '
                    # The `input_file` columns added by these joins will be
                    # NULL if the respective export or allowed declaration is
                    # not loaded (i.e. it's in the cache from some other
                    # invocation).
                    'FULL JOIN window AS ew ON ew.input_file = e.input_file '
                    'FULL JOIN window AS aw ON aw.input_file = a.input_file '
                    'LEFT JOIN active_cond AS c ON a.cond_id = c.nextid '
                    # There may be multiple entries for the same name in the
                    # cache (multiple binaries that implement the same
                    # selector, or different allowlists that allow the same
                    # name. Coalesce the results and only return entries where
                    # an import name has *no* exports found, or an allowed name
                    # comes from at least one loaded file.
                    #
                    # This is sufficient to remove all rows in the common
                    # case--imported API that matches an exported delcaration.
                    # The remaining logic to identify problem is done in Python below.
                    'GROUP BY i.kind, a.kind, i.name, a.name, i.input_file '
                    'HAVING export_found = 0 OR allow_found > 0 '
                    'ORDER BY i.input_file, i.kind, a.kind, i.name, a.name')
        for (arch, import_kind, input_path, import_name,
             allowed_kind, allowlist_paths, allowed_name,
             export_path, export_found, allow_found) in cur.fetchall():
            if import_name and not export_found and not allow_found:
                # Imported but neither exported nor allowed => possible SPI.
                yield MissingName(name=import_name, file=Path(input_path),
                                  arch=arch, kind=import_kind)
            elif not import_name and allow_found:
                # Not imported but allowed => unused allowlist entry to remove.
                # FIXME: split(',') falls apart if an allowlist path contains a
                # comma. We could improve this by using quote() in the query
                # and unquoting here.
                for path in allowlist_paths.split(','):
                    yield UnusedAllowedName(name=allowed_name, file=Path(path),
                                            kind=allowed_kind)
            elif allow_found and export_found:
                # Allowed but also exported => unnecessary allowlist entry to
                # remove.
                for path in allowlist_paths.split(','):
                    yield UnnecessaryAllowedName(name=allowed_name,
                                                 file=Path(path),
                                                 kind=allowed_kind,
                                                 exported_in=Path(export_path))

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
                    cond_id: Optional[int] = None):
        cur = self.con.cursor()
        params = dict(name=name, kind=SYMBOL, file=str(file.resolve()),
                      cond=cond_id)
        cur.execute(dest.statement, params)

    def _add_objc_class(self, name: str, file: Path,
                        dest=InsertionKind.EXPORTS,
                        cond_id: Optional[int] = None):
        cur = self.con.cursor()
        params = dict(name=name, kind=OBJC_CLS, file=str(file.resolve()),
                      cond=cond_id)
        cur.execute(dest.statement, params)

    def _add_objc_selector(self, name: str, class_name: Optional[str],
                           file: Path, dest=InsertionKind.EXPORTS,
                           cond_id: Optional[int] = None):
        cur = self.con.cursor()
        params = dict(name=name, kind=OBJC_SEL, file=str(file.resolve()),
                      cond=cond_id)
        cur.execute(dest.statement, params)
