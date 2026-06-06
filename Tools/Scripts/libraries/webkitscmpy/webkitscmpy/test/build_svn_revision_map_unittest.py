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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import io
import os
import re
import subprocess
import unittest
from unittest import mock

from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Subprocess as MockSubprocess, Time as MockTime
from webkitscmpy import mocks, program
from webkitscmpy.binary_revision_map import ENTRY_SIZE, ZERO_ENTRY
from webkitscmpy.program.build_svn_revision_map import BuildSVNRevisionMap


# Hashes/revisions present in the mock git-repo.json under git_svn=True.
HASHES_BY_REV = {
    1: '9b8311f25a77ba14923d9d5a6532103f54abefcb',  # 1@main
    2: 'fff83bb2d9171b4d9196e977eb0508fd57e7a08d',  # 2@main
    3: 'a30ce8494bf1ac2807a69844f726be4a9843ca55',  # 2.1@branch-a
    4: '1abe25b443e985f93b90d830e4a7e3731336af4d',  # 3@main
    5: '3cd32e352410565bb543821fbf856a6d3caad1c4',  # 2.2@branch-b
    6: '621652add7fc416099bd2063366cc38ff61afe36',  # 2.2@branch-a
    7: '790725a6d79e28db2ecdde29548d2262c0bd059d',  # 2.3@branch-b
    8: 'bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',  # 4@main
    9: 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc',  # 5@main
}

SVN_ID_TRAILER = (
    'git-svn-id: https://svn.example.org/repository/repository/trunk@{rev} '
    '268f45cc-cd09-0410-ab3c-d52691b4dbfc'
)


def _record_for(hash, rev, message='Commit'):
    body = '{message}\n\n{trailer}\n'.format(
        message=message,
        trailer=SVN_ID_TRAILER.format(rev=rev),
    )
    return '{hash}\n{body}'.format(hash=hash, body=body).encode('utf-8')


def _read_slots(path, count):
    with open(path, 'rb') as f:
        data = f.read()
    return [data[i * ENTRY_SIZE:(i + 1) * ENTRY_SIZE] for i in range(count)]


class TestBuildSVNRevisionMap(testing.PathTestCase):
    """End-to-end tests driven by the natural mocks.local.Git infrastructure."""

    basepath = 'mock/repository'

    def setUp(self):
        super().setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, 'metadata'))

    def _for_each_ref_route(self, mock_git):
        """Mock route handling `for-each-ref --format=%(refname) <pattern>`.

        The default mock recognises `--format` and `%(refname)` as separate
        args, but build-svn-revision-map joins them as `--format=%(refname)`.
        """
        def gen(*args, **kwargs):
            # args: (git, 'for-each-ref', '--format=%(refname)', '<pattern>')
            pattern = args[3]
            return mock_git.for_each_ref('%(refname)', None, pattern)
        return MockSubprocess.Route(
            mock_git.executable, 'for-each-ref', '--format=%(refname)',
            re.compile(r'.+'),
            cwd=self.path,
            generator=gen,
        )

    def _run(self, *, args=None):
        """Invoke the build-svn-revision-map subcommand against the natural mock."""
        with mocks.local.Git(self.path, git_svn=True) as mock_git, mocks.local.Svn(), MockTime, OutputCapture() as captured:
            # The mock auto-creates metadata/svn-revision-to-hash.bin so other
            # tests in the suite can rely on revision lookups. This test class
            # exists to verify that the subcommand itself produces that file,
            # so delete it before invoking the subcommand under test.
            output_path = os.path.join(self.path, 'metadata', 'svn-revision-to-hash.bin')
            if os.path.exists(output_path):
                os.unlink(output_path)
            with MockSubprocess(self._for_each_ref_route(mock_git)):
                rc = program.main(
                    args=tuple(args) if args is not None else ('build-svn-revision-map',),
                    path=self.path,
                )
        return rc, captured

    def test_invalid_remote_name(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            rc = program.main(
                args=('build-svn-revision-map', '--remote', 'bad name!'),
                path=self.path,
            )
        self.assertEqual(rc, 1)
        self.assertIn("Invalid remote name", captured.stderr.getvalue())

    def test_writes_full_map(self):
        # The mock's natural git_svn=True log produces all 9 revisions.
        output_path = os.path.join(self.path, 'metadata', 'svn-revision-to-hash.bin')

        rc, _ = self._run()

        self.assertEqual(rc, 0)
        self.assertTrue(os.path.isfile(output_path))
        self.assertEqual(os.path.getsize(output_path), 9 * ENTRY_SIZE)

        slots = _read_slots(output_path, 9)
        for rev, hash in HASHES_BY_REV.items():
            self.assertEqual(slots[rev - 1], bytes.fromhex(hash),
                             msg='slot for r{} did not match expected hash'.format(rev))

    def test_lock_file_is_cleaned_up_on_success(self):
        output_path = os.path.join(self.path, 'metadata', 'svn-revision-to-hash.bin')
        lock_path = output_path + '.lock'

        rc, _ = self._run()

        self.assertEqual(rc, 0)
        self.assertTrue(os.path.isfile(output_path))
        # On success, .lock is renamed to the output path; no stale .lock remains.
        self.assertFalse(os.path.exists(lock_path))

    def test_concurrent_invocation_refuses_to_run(self):
        # Pre-create the .lock file to simulate an in-flight invocation.
        output_path = os.path.join(self.path, 'metadata', 'svn-revision-to-hash.bin')
        lock_path = output_path + '.lock'
        with open(lock_path, 'w') as f:
            f.write('')

        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture() as captured:
            rc = program.main(
                args=('build-svn-revision-map',),
                path=self.path,
            )

        self.assertEqual(rc, 1)
        self.assertIn('another build-svn-revision-map is in progress', captured.stderr.getvalue())
        # We didn't own the lock — it should remain in place.
        self.assertTrue(os.path.exists(lock_path))

    def test_no_warning_when_namespace_is_clean(self):
        # Natural mock has no refs under refs/remote-tags/origin/, so no
        # stale-ref warning should fire on a fresh run.
        rc, captured = self._run()

        self.assertEqual(rc, 0)
        self.assertNotIn('stale ref', captured.root.log.getvalue())

    def test_stale_refs_in_namespace_emit_warning(self):
        # Patch _count_stale_refs directly: simulating "there were 3 leftover
        # refs from a prior killed run" without having to teach the mock
        # how to model a populated refs/remote-tags/<remote>/* namespace.
        with mock.patch.object(BuildSVNRevisionMap, '_count_stale_refs', return_value=3):
            rc, captured = self._run()

        self.assertEqual(rc, 0)
        self.assertIn(
            '3 stale ref(s) under refs/remote-tags/origin/',
            captured.root.log.getvalue(),
        )


class TestCountStaleRefs(unittest.TestCase):
    """Unit tests for ``BuildSVNRevisionMap._count_stale_refs``."""

    @staticmethod
    def _patched_run(returncode=0, stdout=''):
        return mock.patch(
            'webkitscmpy.program.build_svn_revision_map.subprocess.run',
            return_value=subprocess.CompletedProcess(
                args=[], returncode=returncode, stdout=stdout, stderr='',
            ),
        )

    def test_zero_for_empty_namespace(self):
        with self._patched_run(stdout=''):
            count = BuildSVNRevisionMap._count_stale_refs('git', '/repo', 'origin')
        self.assertEqual(count, 0)

    def test_counts_each_ref(self):
        stdout = 'refs/remote-tags/origin/v1\nrefs/remote-tags/origin/v2\n'
        with self._patched_run(stdout=stdout):
            count = BuildSVNRevisionMap._count_stale_refs('git', '/repo', 'origin')
        self.assertEqual(count, 2)

    def test_skips_blank_lines(self):
        with self._patched_run(stdout='\nrefs/remote-tags/origin/v1\n\n'):
            count = BuildSVNRevisionMap._count_stale_refs('git', '/repo', 'origin')
        self.assertEqual(count, 1)

    def test_zero_on_for_each_ref_failure(self):
        # If for-each-ref itself fails, suppress the diagnostic and let the
        # subsequent fetch surface the real error.
        with self._patched_run(returncode=128, stdout=''):
            count = BuildSVNRevisionMap._count_stale_refs('git', '/repo', 'origin')
        self.assertEqual(count, 0)


class TestProcessRecord(unittest.TestCase):
    """Unit tests for ``BuildSVNRevisionMap._process_record``.

    Edge cases that aren't naturally expressible through the mocks.local.Git
    commit DB (gaps in revision numbering, two commits sharing a revision)
    are exercised directly against the parser.
    """

    def test_records_collect_revision_to_hash(self):
        mapping = {}
        for rev in (1, 4, 9):
            BuildSVNRevisionMap._process_record(
                _record_for(HASHES_BY_REV[rev], rev), mapping,
            )
        self.assertEqual(set(mapping.keys()), {1, 4, 9})
        self.assertEqual(mapping[1], [bytes.fromhex(HASHES_BY_REV[1])])
        self.assertEqual(mapping[4], [bytes.fromhex(HASHES_BY_REV[4])])
        self.assertEqual(mapping[9], [bytes.fromhex(HASHES_BY_REV[9])])

    def test_two_commits_for_same_revision_accumulate(self):
        mapping = {}
        BuildSVNRevisionMap._process_record(_record_for(HASHES_BY_REV[5], 5), mapping)
        BuildSVNRevisionMap._process_record(_record_for(HASHES_BY_REV[3], 5), mapping)
        self.assertEqual(set(mapping.keys()), {5})
        self.assertEqual(
            mapping[5],
            [bytes.fromhex(HASHES_BY_REV[5]), bytes.fromhex(HASHES_BY_REV[3])],
        )

    def test_record_without_svn_id_trailer_is_ignored(self):
        body_bytes = '{}\nNo trailer here\n'.format(HASHES_BY_REV[1]).encode('utf-8')
        mapping = {}
        BuildSVNRevisionMap._process_record(body_bytes, mapping)
        self.assertEqual(mapping, {})

    def test_record_with_multiple_svn_id_trailers_raises(self):
        body = (
            'Subject\n\n'
            + SVN_ID_TRAILER.format(rev=1) + '\n'
            + SVN_ID_TRAILER.format(rev=2) + '\n'
        )
        record = '{}\n{}'.format(HASHES_BY_REV[1], body).encode('utf-8')
        with self.assertRaises(ValueError):
            BuildSVNRevisionMap._process_record(record, {})


class TestWriteMap(unittest.TestCase):
    """Unit tests for ``BuildSVNRevisionMap._write_map``.

    Exercises slot layout: missing revisions zero-pad, unique revisions get
    their SHA1, revisions with two-or-more commits zero out.
    """

    def _write(self, mapping, max_slots):
        # _write_map relies on os.fsync(fileno()) so a real file is required.
        # We use BytesIO with a fake fileno() since fsync is harmless to mock.
        buf = io.BytesIO()
        original_fsync = os.fsync
        os.fsync = lambda _fd: None
        try:
            buf.fileno = lambda: -1
            max_rev, non_null = BuildSVNRevisionMap._write_map(mapping, buf)
        finally:
            os.fsync = original_fsync
        data = buf.getvalue()
        return max_rev, non_null, [data[i * ENTRY_SIZE:(i + 1) * ENTRY_SIZE] for i in range(max_slots)]

    def test_missing_revisions_zero_pad(self):
        mapping = {
            1: [bytes.fromhex(HASHES_BY_REV[1])],
            4: [bytes.fromhex(HASHES_BY_REV[4])],
            9: [bytes.fromhex(HASHES_BY_REV[9])],
        }
        max_rev, non_null, slots = self._write(mapping, 9)
        self.assertEqual(max_rev, 9)
        self.assertEqual(non_null, 3)
        self.assertEqual(slots[0], bytes.fromhex(HASHES_BY_REV[1]))
        self.assertEqual(slots[3], bytes.fromhex(HASHES_BY_REV[4]))
        self.assertEqual(slots[8], bytes.fromhex(HASHES_BY_REV[9]))
        for idx in (1, 2, 4, 5, 6, 7):
            self.assertEqual(slots[idx], ZERO_ENTRY,
                             msg='slot index {} should be null'.format(idx))

    def test_two_commits_for_same_revision_zero_out(self):
        mapping = {
            1: [bytes.fromhex(HASHES_BY_REV[1])],
            5: [bytes.fromhex(HASHES_BY_REV[5]), bytes.fromhex(HASHES_BY_REV[3])],
            9: [bytes.fromhex(HASHES_BY_REV[9])],
        }
        max_rev, non_null, slots = self._write(mapping, 9)
        self.assertEqual(max_rev, 9)
        self.assertEqual(non_null, 2)
        self.assertEqual(slots[0], bytes.fromhex(HASHES_BY_REV[1]))
        self.assertEqual(slots[8], bytes.fromhex(HASHES_BY_REV[9]))
        # Two commits sharing r5 → null slot.
        self.assertEqual(slots[4], ZERO_ENTRY)

    def test_empty_mapping_writes_nothing(self):
        max_rev, non_null, slots = self._write({}, 0)
        self.assertEqual(max_rev, 0)
        self.assertEqual(non_null, 0)
        self.assertEqual(slots, [])
