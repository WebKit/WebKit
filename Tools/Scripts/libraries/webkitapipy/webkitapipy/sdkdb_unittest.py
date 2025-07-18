# Copyright (C) 2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import tempfile
from pathlib import Path
from unittest import TestCase

from .allow import AllowList
from .sdkdb import OBJC_CLS, OBJC_SEL, SDKDB, SYMBOL, MissingName, UnnecessaryAllowedName, UnusedAllowedName
from .macho import APIReport

# Fixtures:
F = Path('/libdoesntexist.dylib')
F_Hash = 1234567890
F_NonNormalized = Path('/foo/../libdoesntexist.dylib')
R = APIReport(
    file=F, arch='arm64e',
    exports={'_WKDoesntExistLibraryVersion', '_OBJC_CLASS_$_WKDoesntExist'},
    methods={'initWithData:'}
)

F_Client = Path('/libdoesntexist_client.dylib')
R_Client = APIReport(
    file=F_Client, arch='arm64e',
    imports={'_WKDoesntExistLibraryVersion', '_OBJC_CLASS_$_WKDoesntExist'},
    selrefs={'initWithData:'}
)
R_MissingSymbol = MissingName(name='_WKDoesntExistLibraryVersion',
                              file=F_Client, arch='arm64e', kind=SYMBOL)
R_MissingClass = MissingName(name='WKDoesntExist', file=F_Client, arch='arm64e', kind=OBJC_CLS)
R_MissingSelector = MissingName(name='initWithData:', file=F_Client, arch='arm64e', kind=OBJC_SEL)

A = AllowList.from_dict({'sdkdb-unittest':
                         {'rdar://12345':
                          {'classes': ['WKDoesntExist'],
                           'selectors': ['initWithData:'],
                           'symbols': ['_WKDoesntExistLibraryVersion']}}})
A_File = Path('/allowed.toml')
A_Hash = 23456
A_UnusedAllow = UnusedAllowedName(name='WKDoesntExist', file=A_File,
                                  kind=OBJC_CLS)
A_AllowedAPI = UnnecessaryAllowedName(name='WKDoesntExist', file=A_File,
                                      kind=OBJC_CLS, exported_in=F)

R_Uses_Own_Selector = APIReport(
    file=F_Client, arch='arm64e',
    methods={'someInternalMethodWithObject:'},
    selrefs={'someInternalMethodWithObject:'}
)

A_Conditional = AllowList.from_dict(
    {'sdkdb-unittest': {'rdar://12345': {
        'classes': ['WKDoesntExist'],
        'selectors': ['initWithData:'],
        'symbols': ['_WKDoesntExistLibraryVersion'],
        'requires': ['ENABLE_FEATURE']}
    }}
)

A_NegatedConditional = AllowList.from_dict(
    {'sdkdb-unittest': {'rdar://12345': {
        'classes': ['WKDoesntExist'],
        'selectors': ['initWithData:'],
        'symbols': ['_WKDoesntExistLibraryVersion'],
        'requires': ['!ENABLE_FEATURE']}
    }}
)

A_MultipleConditions = AllowList.from_dict(
    {'sdkdb-unittest': {'rdar://12345': {
        'classes': ['WKDoesntExist'],
        'selectors': ['initWithData:'],
        'symbols': ['_WKDoesntExistLibraryVersion'],
        'requires': ['ENABLE_A', 'ENABLE_B', '!ENABLE_C']}
    }}
)

class TestSDKDB(TestCase):
    def setUp(self):
        self.dbfile = tempfile.NamedTemporaryFile(prefix='TestSDKDB-')
        self.sdkdb = SDKDB(Path(self.dbfile.name))

    def add_library(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(F, F_Hash)
            self.sdkdb._add_api_report(R, F)

    def add_allowlist(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(A_File, A_Hash)
            self.sdkdb._add_allowlist(A, A_File)

    def reconnect(self):
        self.sdkdb = SDKDB(Path(self.dbfile.name))

    def audit_with(self, fixture: APIReport):
        self.sdkdb.add_for_auditing(fixture)
        return self.sdkdb.audit()

    def assertEmpty(self, seq):
        self.assertEqual([], list(seq))

    def test_ingests_api_report_nonnormalized(self):
        # Given a file added to the cache:
        with self.sdkdb:
            self.assertFalse(self.sdkdb._cache_hit_preparing_to_insert(F_NonNormalized,
                                                                       F_Hash))
            # Insertions should not cause a foreign key constraint violation:
            self.sdkdb._add_api_report(R, F_NonNormalized)

        # Its symbols, classes, and implemented selectors should be added:
        self.assertEmpty(self.audit_with(R_Client))

    def test_path_normalized_when_cache_hit(self):
        with self.sdkdb:
            self.assertFalse(self.sdkdb._cache_hit_preparing_to_insert(F, F_Hash))
            self.assertTrue(self.sdkdb._cache_hit_preparing_to_insert(F_NonNormalized, F_Hash))

    def test_path_normalized_when_cache_miss(self):
        with self.sdkdb:
            self.assertFalse(self.sdkdb._cache_hit_preparing_to_insert(F_NonNormalized, F_Hash))
            self.assertTrue(self.sdkdb._cache_hit_preparing_to_insert(F, F_Hash))

    def test_entries_removed_when_binary_updated(self):
        # Given a file added to the cache:
        self.add_library()

        # When it is replaced with a new version that contains different exports...
        new_report = APIReport(file=F, arch='arm64e', exports=set(), methods=set())
        new_hash = F_Hash + 1
        with self.sdkdb:
            self.assertFalse(self.sdkdb._cache_hit_preparing_to_insert(F, new_hash))
            self.sdkdb._add_api_report(new_report, new_hash)

        # ...the old exports should be removed:
        diagnostics = set(self.audit_with(R_Client))
        self.assertEqual({R_MissingSymbol, R_MissingClass, R_MissingSelector},
                         diagnostics)

    def test_audit_missing_name_from_spi(self):
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_allowed_name_from_spi(self):
        self.add_allowlist()
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_allowed_conditional(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(A_File, A_Hash)
            self.sdkdb._add_allowlist(A_Conditional, A_File)
        self.sdkdb.add_defines(['ENABLE_FEATURE'])
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_missing_name_conditional(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(A_File, A_Hash)
            self.sdkdb._add_allowlist(A_Conditional, A_File)
        self.sdkdb.add_defines(['OTHER_FEATURE'])
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_missing_name_negated_conditional(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(A_File, A_Hash)
            self.sdkdb._add_allowlist(A_NegatedConditional, A_File)
        self.sdkdb.add_defines(['ENABLE_FEATURE'])
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_allowed_negated_conditional(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(A_File, A_Hash)
            self.sdkdb._add_allowlist(A_NegatedConditional, A_File)
        self.sdkdb.add_defines(['OTHER_FEATURE'])
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_allowed_multiple_conditions(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(A_File, A_Hash)
            self.sdkdb._add_allowlist(A_MultipleConditions, A_File)
        self.sdkdb.add_defines(['ENABLE_A', 'ENABLE_B'])
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_missing_name_multiple_conditions(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(A_File, A_Hash)
            self.sdkdb._add_allowlist(A_MultipleConditions, A_File)
        self.sdkdb.add_defines(['ENABLE_A'])
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_missing_name_multiple_conditions_negation(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(A_File, A_Hash)
            self.sdkdb._add_allowlist(A_MultipleConditions, A_File)
        self.sdkdb.add_defines(['ENABLE_A', 'ENABLE_B', 'ENABLE_C'])
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_api_from_loaded_file(self):
        self.add_library()
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_missing_name_from_unloaded_file(self):
        self.add_library()
        self.reconnect()
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_allowed_name_from_unloaded_file(self):
        self.add_library()
        self.reconnect()
        self.add_allowlist()
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_unused_allow_from_api(self):
        self.add_library()
        self.add_allowlist()
        self.assertIn(A_AllowedAPI, self.audit_with(R_Client))

    def test_audit_missing_name_from_unloaded_file_unloaded_allowlist(self):
        self.add_library()
        self.add_allowlist()
        self.reconnect()
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_allowed_own_methods(self):
        self.assertEmpty(self.audit_with(R_Uses_Own_Selector))

    def test_audit_unused_allow_from_loaded_allowlist(self):
        self.add_allowlist()
        self.assertIn(A_UnusedAllow, self.sdkdb.audit())

    def test_audit_no_unused_allow_from_unloaded_allowlist(self):
        self.add_allowlist()
        self.reconnect()
        self.assertEmpty(self.sdkdb.audit())

    def test_audit_api_in_loaded_and_unloaded_library(self):
        other_library = APIReport(file=Path('/libunrelated.dylib'),
                                  arch='arm64e', methods={'initWithData:'})
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(other_library.file, 23456789)
            self.sdkdb._add_api_report(other_library, other_library.file)

        self.reconnect()
        self.add_library()
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_allowed_name_in_loaded_and_unloaded_allowlist(self):
        self.add_allowlist()
        self.reconnect()

        other_allowlist = AllowList.from_dict({'test': {'legacy': {'selectors': ['initWithData:']}}})
        other_file = Path('/allowed2.toml')
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(other_file, 34567890)
            self.sdkdb._add_allowlist(other_allowlist, other_file)

        self.assertNotIn(R_MissingSelector, self.audit_with(R_Client))

    def test_audit_unnecessary_allow_in_loaded_and_unloaded_allowlist(self):
        self.add_allowlist()
        self.reconnect()
        self.add_library()

        other_allowlist = AllowList.from_dict({'test': {'legacy': {'selectors': ['initWithData:']}}})
        other_file = Path('/allowed2.toml')
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(other_file, 34567890)
            self.sdkdb._add_allowlist(other_allowlist, other_file)

        diagnostics = list(self.audit_with(R_Client))
        self.assertIn(UnnecessaryAllowedName(name='initWithData:',
                                             kind=OBJC_SEL, file=other_file,
                                             exported_in=F), diagnostics)
        self.assertNotIn(UnnecessaryAllowedName(name='initWithData:',
                                                kind=OBJC_SEL, file=A_File,
                                                exported_in=F), diagnostics)

    def test_audit_unused_allow_multiple_allowlists(self):
        self.add_allowlist()
        self.add_library()

        other_allowlist = AllowList.from_dict({'test': {'legacy': {'selectors': ['initWithData:']}}})
        other_file = Path('/allowed2.toml')
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(other_file, 34567890)
            self.sdkdb._add_allowlist(other_allowlist, other_file)

        diagnostics = list(self.audit_with(R_Client))
        self.assertIn(UnnecessaryAllowedName(name='initWithData:',
                                             kind=OBJC_SEL, file=other_file,
                                             exported_in=F), diagnostics)
        self.assertIn(UnnecessaryAllowedName(name='initWithData:',
                                             kind=OBJC_SEL, file=A_File,
                                             exported_in=F), diagnostics)
