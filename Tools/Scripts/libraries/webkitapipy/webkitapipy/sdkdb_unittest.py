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
R_Selector = APIReport.Selector('initWithData:', 'WKDoesntExist')
R = APIReport(
    file=F, arch='arm64e',
    exports={'_WKDoesntExistLibraryVersion', '_OBJC_CLASS_$_WKDoesntExist'},
    methods={R_Selector},
    platform='iOS', min_os='1.0', sdk='1.0'
)

F_Client = Path('/libdoesntexist_client.dylib')
R_Client = APIReport(
    file=F_Client, arch='arm64e',
    imports={'_WKDoesntExistLibraryVersion', '_OBJC_CLASS_$_WKDoesntExist'},
    selrefs={'initWithData:'},
    platform='iOS', min_os='1.0', sdk='1.0'
)
R_MissingSymbol = MissingName(name='_WKDoesntExistLibraryVersion',
                              file=F_Client, arch='arm64e', kind=SYMBOL)
R_MissingClass = MissingName(name='WKDoesntExist', file=F_Client, arch='arm64e', kind=OBJC_CLS)
R_MissingSelector = MissingName(name='initWithData:', file=F_Client, arch='arm64e', kind=OBJC_SEL)

A = AllowList.from_dict({'temporary-usage': [
    {'request': 'rdar://12345',
     'cleanup': 'rdar://12346',
     'classes': ['WKDoesntExist'],
     'selectors': [{'name': 'initWithData:', 'class': '?'}],
     'symbols': ['WKDoesntExistLibraryVersion']}
]})
A_File = Path('/allowed.toml')
A_Hash = 23456
A_ExplicitlyAllowUnused = AllowList.from_dict({'temporary-usage': [
    {'request': 'rdar://12345',
     'cleanup': 'rdar://12346',
     'classes': ['WKDoesntExist'],
     'selectors': [{'name': 'initWithData:', 'class': '?'}],
     'symbols': ['WKDoesntExistLibraryVersion'],
     'allow-unused': True}
]})

A_UnusedAllow = UnusedAllowedName(name='WKDoesntExist', file=A_File,
                                  kind=OBJC_CLS)
A_AllowedAPI = UnnecessaryAllowedName(name='WKDoesntExist', file=A_File,
                                      kind=OBJC_CLS, exported_in=F)

R_Uses_Own_Selector = APIReport(
    file=F_Client, arch='arm64e',
    methods={APIReport.Selector('someInternalMethodWithObject:', 'Class')},
    selrefs={'someInternalMethodWithObject:'},
    platform='iOS', min_os='1.0', sdk='1.0'
)

A_Conditional = AllowList.from_dict({'temporary-usage': [
    {'request': 'rdar://12345',
     'cleanup': 'rdar://12346',
     'classes': ['WKDoesntExist'],
     'selectors': [{'name': 'initWithData:', 'class': '?'}],
     'symbols': ['WKDoesntExistLibraryVersion'],
     'requires': ['ENABLE_FEATURE'],
     'requires-os': ['iOS>=1.0'],
     'requires-sdk': ['iOS < 99']}
]})

A_NegatedConditional = AllowList.from_dict({'temporary-usage': [
    {'request': 'rdar://12345',
     'cleanup': 'rdar://12346',
     'classes': ['WKDoesntExist'],
     'selectors': [{'name': 'initWithData:', 'class': '?'}],
     'symbols': ['WKDoesntExistLibraryVersion'],
     'requires': ['!ENABLE_FEATURE']}
]})

A_MultipleConditions = AllowList.from_dict({'temporary-usage': [
    {'request': 'rdar://12345',
     'cleanup': 'rdar://12346',
     'classes': ['WKDoesntExist'],
     'selectors': [{'name': 'initWithData:', 'class': '?'}],
     'symbols': ['WKDoesntExistLibraryVersion'],
     'requires': ['ENABLE_A', 'ENABLE_B', '!ENABLE_C']}
]})

A_QualifiedSelector = AllowList.from_dict({'temporary-usage': [
    {'request': 'rdar://12345',
     'cleanup': 'rdar://12346',
     'classes': ['WKDoesntExist'],
     'selectors': [{'name': 'initWithData:', 'class': 'WKDoesntExist'}],
     'symbols': ['WKDoesntExistLibraryVersion']}
]})

A_NonmatchingOS = AllowList.from_dict({'temporary-usage': [
    {'request': 'rdar://12345',
     'cleanup': 'rdar://12346',
     'classes': ['WKDoesntExist'],
     'selectors': [{'name': 'initWithData:', 'class': '?'}],
     'symbols': ['WKDoesntExistLibraryVersion'],
     'requires': ['ENABLE_FEATURE'],
     'requires-os': ['iOS>=99.0']}
]})

S = {
    'PublicSDKContentRoot': [{
        'target': 'arm64-apple-ios18.5',
        'interfaces': [{
            'name': 'NSData',
            'access': 'public',
            'instanceMethods': [
                {'access': 'public', 'name': 'initWithData:'},
            ],
        }]
    }]
}
S_File = Path('/Foundation.partial.sdkdb')
S_Hash = 3456789
S_UnnecessarySelector = UnnecessaryAllowedName(name='initWithData:', file=A_File, kind=OBJC_SEL, exported_in=S_File)

class TestSDKDB(TestCase):
    def setUp(self):
        self.dbfile = tempfile.NamedTemporaryFile(prefix='TestSDKDB-')
        self.sdkdb = SDKDB(Path(self.dbfile.name))

    def tearDown(self):
        self.sdkdb.con.close()

    def add_library(self, fixture=R, file=F, file_hash=F_Hash):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(file, file_hash)
            self.sdkdb._add_api_report(fixture, file)

    def add_partial_sdkdb(self):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(S_File, S_Hash)
            self.sdkdb._add_partial_sdkdb(S, S_File, spi=False, abi=False)

    def add_allowlist(self, fixture=A, file=A_File, hash=A_Hash):
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(file, hash)
            self.sdkdb._add_allowlist(fixture, file)

    def reconnect(self):
        self.sdkdb = SDKDB(Path(self.dbfile.name))

    def audit_with(self, fixture: APIReport, file_hash=54321):
        self.sdkdb._cache_hit_preparing_to_insert(fixture.file, file_hash)
        self.sdkdb._add_api_report(fixture, fixture.file)
        self.sdkdb._add_imports(fixture)
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
        new_report = APIReport(file=F, arch='arm64e', exports=set(), methods=set(),
                               platform='iOS', min_os='1.0', sdk='1.0')
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
        self.add_allowlist(A_Conditional)
        self.sdkdb.add_conditions({'ENABLE_FEATURE': 1})
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_missing_name_conditional(self):
        self.add_allowlist(A_Conditional)
        self.sdkdb.add_conditions({'OTHER_FEATURE': 1})
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_missing_name_negated_conditional(self):
        self.add_allowlist(A_NegatedConditional)
        self.sdkdb.add_conditions({'ENABLE_FEATURE': 1})
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_allowed_negated_conditional(self):
        self.add_allowlist(A_NegatedConditional)
        self.sdkdb.add_conditions({'OTHER_FEATURE': 1})
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_allowed_multiple_conditions(self):
        self.add_allowlist(A_MultipleConditions)
        self.sdkdb.add_conditions({'ENABLE_A': 1, 'ENABLE_B': 1})
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_missing_name_multiple_conditions(self):
        self.add_allowlist(A_MultipleConditions)
        self.sdkdb.add_conditions({'ENABLE_A': 1})
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_missing_name_multiple_conditions_negation(self):
        self.add_allowlist(A_MultipleConditions)
        self.sdkdb.add_conditions({'ENABLE_A': 1, 'ENABLE_B': 1, 'ENABLE_C': 1})
        self.assertIn(R_MissingSymbol, self.audit_with(R_Client))

    def test_audit_minos_conditions(self):
        self.add_allowlist(A_NonmatchingOS)
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

    def test_audit_unnecessary_allow_from_selector(self):
        self.add_partial_sdkdb()
        self.add_allowlist()
        self.assertIn(S_UnnecessarySelector, self.audit_with(R_Client))

    def test_audit_allowed_fully_qualified_selector(self):
        self.add_partial_sdkdb()
        self.add_allowlist(A_QualifiedSelector)
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_allowed_fully_qualified_selector_with_bystander(self):
        # Same test as above but with an additional, unloaded, unrelated
        # library exporting a method with the same selector.
        bystander = APIReport(file=F, arch='arm64e',
                              platform='iOS', min_os='1.0', sdk='1.0',
                              methods={APIReport.Selector('initWithData:',
                                                          'UnrelatedClass')})
        self.add_library(bystander, file=F, file_hash=F_Hash)
        self.reconnect()

        self.add_partial_sdkdb()  # -[NSData initWithData:]
        self.add_allowlist(A_QualifiedSelector)  # -[WKDoesntExist initWithData:]
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_allowed_fully_qualified_selector_with_different_class_unloaded(self):
        # Add -[NSData initWithData:] to the exports table, then reconnect to
        # move it out of the window.
        self.add_partial_sdkdb()
        self.reconnect()
        # Add -[WKDoesntExist initWithData:] to the allow table.
        self.add_allowlist(A_QualifiedSelector)
        # A client that sends "initWithData:" should have no audit issues
        # (it's covered by the WKDoesntExist entry).
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_unused_allow_from_loaded_allowlist(self):
        self.add_allowlist()
        self.assertIn(A_UnusedAllow, self.sdkdb.audit())

    def test_audit_no_unused_when_explicitly_allowed(self):
        self.add_allowlist(A_ExplicitlyAllowUnused)
        self.assertNotIn(A_UnusedAllow, self.sdkdb.audit())

    def test_audit_unused_allow_from_conflicting_allow_unused_keys(self):
        self.add_allowlist()
        self.add_allowlist(A_ExplicitlyAllowUnused)
        self.assertIn(A_UnusedAllow, self.sdkdb.audit())

    def test_audit_no_unused_allow_from_unloaded_allowlist(self):
        self.add_allowlist()
        self.reconnect()
        self.assertEmpty(self.sdkdb.audit())

    def test_audit_api_in_loaded_and_unloaded_library(self):
        # When two libraries which implement the same method are in the cache,
        # and one is unloaded, the other method should still be matched.
        other_library = APIReport(file=Path('/libunrelated.dylib'),
                                  arch='arm64e', methods={R_Selector},
                                  platform='iOS', min_os='1.0', sdk='1.0')
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(other_library.file, 23456789)
            self.sdkdb._add_api_report(other_library, other_library.file)

        self.reconnect()
        self.add_library()
        self.assertEmpty(self.audit_with(R_Client))

    def test_audit_allowed_name_in_loaded_and_unloaded_allowlist(self):
        self.add_allowlist()
        self.reconnect()

        other_allowlist = AllowList.from_dict(
            {'legacy': [{'selectors': [{'name': 'initWithData:',
                                        'class': '?'}]}]})
        other_file = Path('/allowed2.toml')
        with self.sdkdb:
            self.sdkdb._cache_hit_preparing_to_insert(other_file, 34567890)
            self.sdkdb._add_allowlist(other_allowlist, other_file)

        self.assertNotIn(R_MissingSelector, self.audit_with(R_Client))

    def test_audit_unnecessary_allow_in_loaded_and_unloaded_allowlist(self):
        self.add_allowlist()
        self.reconnect()
        self.add_library()

        other_allowlist = AllowList.from_dict(
            {'legacy': [{'selectors': [{'name': 'initWithData:',
                                                'class': '?'}]}]})
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

        other_allowlist = AllowList.from_dict(
            {'legacy': [{'selectors': [{'name': 'initWithData:',
                                                'class': '?'}]}]})
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

    def test_audit_allow_different_fully_qualified_methods_same_name(self):
        allowlist = AllowList.from_dict({'temporary-usage': [
            {'request': 'rdar://12345',
             'cleanup': 'rdar://12346',
             'classes': ['WKDoesntExist'],
             'selectors': [{'name': 'initWithData:', 'class': 'WKDoesntExist'}],
             'symbols': ['WKDoesntExistLibraryVersion']}
        ]})
        self.add_allowlist(allowlist)
        client = APIReport(file=F_Client, arch='arm64e',
                           platform='iOS', min_os='1.0', sdk='1.0',
                           methods={APIReport.Selector('initWithData:',
                                                       'UnrelatedClass')},
                           imports={'_WKDoesntExistLibraryVersion',
                                    '_OBJC_CLASS_$_WKDoesntExist'},
                           selrefs={'initWithData:'})
        self.assertEmpty(self.audit_with(client))

    def test_audit_unnecessary_allow_unqualified_methods_same_name(self):
        self.add_allowlist()
        client = APIReport(file=F_Client, arch='arm64e',
                           platform='iOS', min_os='1.0', sdk='1.0',
                           methods={APIReport.Selector('initWithData:',
                                                       'UnrelatedClass')},
                           imports={'_WKDoesntExistLibraryVersion',
                                    '_OBJC_CLASS_$_WKDoesntExist'},
                           selrefs={'initWithData:'})
        self.assertIn(UnnecessaryAllowedName(name='initWithData:', file=A_File,
                                             kind=OBJC_SEL,
                                             exported_in=F_Client),
                      self.audit_with(client))

    def test_exported_in_nonnull(self):
        # Add the same API declarations twice (i.e. two different SDKs loaded
        # into the same cache).
        self.add_library(file=Path('/some/other/sdk/libdoesntexist.dylib'))
        self.add_library()

        # Reconnect and load only the second source of our symbols.
        self.reconnect()
        self.add_library()

        # Cause an UnnecessaryAllowedName. It should have a nonnull
        # export_path.
        self.add_allowlist()
        self.assertIn(UnnecessaryAllowedName(name='initWithData:',
                                             kind=OBJC_SEL, file=A_File,
                                             exported_in=F),
                      self.audit_with(R_Client))
