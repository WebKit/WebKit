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

import contextlib
import os
import re
import subprocess
import sys
import tempfile

from .command import Command
from webkitscmpy import Commit, ScmBase, local, log
from webkitscmpy.binary_revision_map import ENTRY_SIZE, ZERO_ENTRY


REMOTE_NAME_RE = re.compile(r'^[a-zA-Z0-9._-]+$')


class BuildSVNRevisionMap(Command):
    name = 'build-svn-revision-map'
    help = 'Build a binary map from Subversion revision numbers to git commit hashes'
    aliases = []

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--remote', '-r',
            dest='remote',
            default='origin',
            help='Git remote whose tags and branches to scan (default: origin)',
        )
        parser.add_argument(
            '--output', '-o',
            dest='output',
            default=None,
            help='Path to write the binary revision-to-hash map (defaults to '
                 '<repository-metadata>/svn-revision-to-hash.bin)',
        )

    @classmethod
    def _for_each_ref(cls, git, root, pattern):
        """Run ``git for-each-ref --format=%(refname) <pattern>`` and return the CompletedProcess."""
        return subprocess.run(
            [git, 'for-each-ref', '--format=%(refname)', pattern],
            cwd=root, capture_output=True, text=True,
        )

    @classmethod
    def _delete_temp_refs(cls, git, root, remote):
        """Atomically delete every ref under refs/remote-tags/<remote>/.

        Drives `git update-ref -z --stdin` directly so we control the NUL
        framing exactly. Piping `for-each-ref`'s output doesn't work: its
        ``--format`` has no "no record separator" mode, so a stray LF leaks
        into update-ref's stream between records.
        """
        try:
            listing = cls._for_each_ref(git, root, 'refs/remote-tags/{}/'.format(remote))
            if listing.returncode != 0:
                log.warning('Failed to list refs/remote-tags/{}/ for cleanup: {}'.format(
                    remote, listing.stderr,
                ))
                return
            refs = [r for r in listing.stdout.splitlines() if r]
            if not refs:
                return
            # `delete SP <ref> NUL [<old-oid>] NUL` per ref; we hold the lock
            # so racing is impossible — omit <old-oid> to delete unconditionally.
            stdin = ''.join('delete {}\x00\x00'.format(ref) for ref in refs)
            upd = subprocess.run(
                [git, 'update-ref', '-z', '--stdin'],
                cwd=root, input=stdin, capture_output=True, text=True,
            )
            if upd.returncode != 0:
                log.warning('Failed to delete temporary refs under refs/remote-tags/{}/: {}'.format(
                    remote, upd.stderr,
                ))
        except Exception as e:
            log.warning('Failed to clean up temporary refs under refs/remote-tags/{}/: {}'.format(remote, e))

    @classmethod
    def _count_stale_refs(cls, git, root, remote):
        """Return the number of refs already living in refs/remote-tags/<remote>/*
        before the fetch -- a non-zero count means a previous run was killed
        between fetch and cleanup, leaving refs stranded.

        Returns 0 if `for-each-ref` itself fails; the subsequent fetch will
        surface any real git error, so this diagnostic path stays silent.
        """
        result = cls._for_each_ref(git, root, 'refs/remote-tags/{}/'.format(remote))
        if result.returncode != 0:
            return 0
        return sum(1 for line in result.stdout.splitlines() if line)

    @classmethod
    @contextlib.contextmanager
    def _temp_ref_namespace(cls, git, root, remote):
        """Manage the refs/remote-tags/<remote>/* namespace.

        On exit, atomically deletes every ref currently in the namespace,
        even if the body raises or is interrupted. Stale refs left over from
        a killed prior run are pruned by the ``--prune`` flag on the
        subsequent ``git fetch`` (see main()) -- since the refspec destination
        is ``refs/remote-tags/<remote>/*``, --prune drops any ref in that
        namespace whose corresponding source ref no longer exists on the
        remote, which is exactly what we'd otherwise re-implement here.
        """
        stale = cls._count_stale_refs(git, root, remote)
        if stale:
            log.warning(
                'Found {} stale ref(s) under refs/remote-tags/{}/ from a prior '
                'killed run; git fetch --prune will drop them.'.format(stale, remote),
            )
        try:
            yield
        finally:
            cls._delete_temp_refs(git, root, remote)

    @classmethod
    def _enumerate_refs(cls, git, root, remote):
        """Return the list of refs under refs/remotes/<remote>/* and refs/remote-tags/<remote>/*."""
        refs = []
        for pattern in (
            'refs/remotes/{}/'.format(remote),
            'refs/remote-tags/{}/'.format(remote),
        ):
            result = cls._for_each_ref(git, root, pattern)
            if result.returncode != 0:
                raise RuntimeError("git for-each-ref failed for '{}' (exit {}): {}".format(
                    pattern, result.returncode, result.stderr,
                ))
            for line in result.stdout.splitlines():
                line = line.strip()
                if line:
                    refs.append(line)
        return refs

    @classmethod
    def _build_mapping(cls, git, root, refs):
        """Stream `git log --format='%H%n%B%x00' <refs>` and accumulate {revision: [sha20, ...]}."""
        mapping = {}
        with tempfile.TemporaryFile(mode='w+', encoding='utf-8', errors='replace') as stderr_tmp:
            proc = subprocess.Popen(
                [git, '--no-replace-objects', 'log', '--format=%H%n%B%x00'] + refs,
                cwd=root, stdout=subprocess.PIPE, stderr=stderr_tmp,
            )
            # Match the convention used in local.Git.Cache._iter_commits: a single
            # poll() before reading lets the local mock infrastructure fill stdout.
            # In production it's a no-op because data already streams from the pipe.
            proc.poll()
            try:
                buf = bytearray()
                for chunk in iter(lambda: proc.stdout.read(65536), b''):
                    buf.extend(chunk)
                    start = 0
                    while True:
                        sep = buf.find(b'\x00', start)
                        if sep < 0:
                            break
                        cls._process_record(bytes(buf[start:sep]), mapping)
                        start = sep + 1
                    del buf[:start]
                if buf.strip():
                    cls._process_record(bytes(buf), mapping)
            finally:
                if proc.poll() is None:
                    proc.kill()
                proc.wait()

            stderr_tmp.seek(0)
            err = stderr_tmp.read()

        if proc.returncode != 0:
            raise RuntimeError("git log failed (exit {}): {}".format(proc.returncode, err))
        if err.strip():
            log.debug('git log stderr: {}'.format(err))
        return mapping

    @classmethod
    def _write_map(cls, mapping, fileobj):
        """Write the packed binary file to ``fileobj``. Returns (max_rev, non_null)."""
        max_rev = max(mapping) if mapping else 0
        if max_rev * ENTRY_SIZE > 10 * 1024 * 1024:
            raise ValueError(
                'Refusing to write {:,} bytes (max revision {:,}); cap is 10 MiB. '
                'A commit may have a malformed git-svn-id trailer; if WebKit has '
                'legitimately grown past the cap, raise the limit in _write_map.'.format(
                    max_rev * ENTRY_SIZE, max_rev,
                )
            )
        non_null = 0
        for rev in range(1, max_rev + 1):
            shas = mapping.get(rev)
            if shas and len(shas) == 1:
                assert len(shas[0]) == ENTRY_SIZE, 'invariant: every mapping entry is {} bytes'.format(ENTRY_SIZE)
                fileobj.write(shas[0])
                non_null += 1
            else:
                fileobj.write(ZERO_ENTRY)
        fileobj.flush()
        os.fsync(fileobj.fileno())
        return max_rev, non_null

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write('build-svn-revision-map requires a local git repository\n')
            return 1

        remote = args.remote
        if not REMOTE_NAME_RE.match(remote):
            sys.stderr.write("Invalid remote name '{}'; must match {}\n".format(
                remote, REMOTE_NAME_RE.pattern,
            ))
            return 1

        output = args.output
        if not output:
            if not repository.metadata:
                sys.stderr.write(
                    'No --output specified and repository has no metadata directory\n'
                )
                return 1
            output = os.path.join(repository.metadata, 'svn-revision-to-hash.bin')

        os.makedirs(os.path.dirname(output) or '.', exist_ok=True)

        lock_path = output + '.lock'
        try:
            lock_fd = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644)
        except FileExistsError:
            sys.stderr.write(
                "another build-svn-revision-map is in progress, or a previous "
                "run was killed without cleanup; remove {} if you're "
                "sure no other invocation is running\n".format(lock_path)
            )
            return 1

        committed = False
        written = False
        try:
            git = repository.executable()
            root = repository.root_path

            # Fetch tags into a private namespace under refs/remote-tags/<remote>/
            # rather than refs/tags/. Two reasons: (a) refs/tags/ may already
            # contain locally-published WebKit release tags, and a
            # refs/tags/*:refs/tags/* refspec would silently overwrite them with
            # the remote's tags; (b) keeping the scan namespace private means
            # we can clean it up unconditionally on exit without touching any
            # ref the user expects to keep.
            #
            # --no-tags suppresses git's default auto-tag-follow, which would
            # otherwise also write each fetched tag into refs/tags/*
            # alongside our explicit refspec destination, defeating reason (a).
            # --prune drops any ref already in refs/remote-tags/<remote>/* whose
            # source tag no longer exists on the remote (i.e. stale leftovers
            # from a killed prior run); the prune scope is bounded to the
            # explicit refspec's destination namespace.
            with cls._temp_ref_namespace(git, root, remote):
                fetch_proc = subprocess.Popen(
                    [git, 'fetch', '--no-tags', '--prune', remote,
                     'refs/tags/*:refs/remote-tags/{}/*'.format(remote)],
                    cwd=root,
                )
                fetch_proc.wait()
                if fetch_proc.returncode != 0:
                    sys.stderr.write("git fetch for remote '{}' failed (exit {})\n".format(
                        remote, fetch_proc.returncode,
                    ))
                    return 1

                refs = cls._enumerate_refs(git, root, remote)
                if not refs:
                    sys.stderr.write(
                        "No refs found under refs/remotes/{remote}/ or "
                        "refs/remote-tags/{remote}/\n".format(remote=remote),
                    )
                mapping = cls._build_mapping(git, root, refs) if refs else {}

            try:
                with os.fdopen(lock_fd, 'wb') as out_fp:
                    lock_fd = None  # ownership transferred to out_fp
                    max_rev, non_null = cls._write_map(mapping, out_fp)
            except ValueError as e:
                sys.stderr.write(str(e) + '\n')
                return 1
            written = True

            try:
                os.replace(lock_path, output)
            except PermissionError as e:
                sys.stderr.write(
                    "Could not replace {}: {}. The new map remains at {}.\n".format(
                        output, e, lock_path,
                    )
                )
                return 1
            committed = True

            print(
                'Wrote {output}: max revision = {max_rev}, non-null = {non_null}'.format(
                    output=output, max_rev=max_rev, non_null=non_null,
                ),
                file=sys.stderr,
            )
            return 0
        finally:
            if not committed and not written:
                if lock_fd is not None:
                    try:
                        os.close(lock_fd)
                    except OSError:
                        pass
                try:
                    os.unlink(lock_path)
                except FileNotFoundError:
                    pass

    @classmethod
    def _process_record(cls, record, mapping):
        record = record.lstrip(b'\n')
        if not record:
            return
        lf = record.find(b'\n')
        if lf < 0:
            hash_bytes = record.strip()
            body = b''
        else:
            hash_bytes = record[:lf].strip()
            body = record[lf + 1:]

        try:
            sha = bytes.fromhex(hash_bytes.decode('ascii'))
        except (ValueError, UnicodeDecodeError):
            log.warning('Skipping malformed git log record (bad hash): {!r}'.format(hash_bytes[:80]))
            return
        if len(sha) != ENTRY_SIZE:
            log.warning('Skipping commit {}: hash length {} != {} (SHA-256 repo?)'.format(
                hash_bytes.decode('ascii', errors='replace'), len(sha), ENTRY_SIZE,
            ))
            return

        trailers = Commit.parse_trailers(body.decode('utf-8', errors='replace'))

        revision = None
        for trailer in trailers:
            m = ScmBase.GIT_SVN_REVISION.match(trailer)
            if not m:
                continue
            parsed_rev = int(m.group('revision'))
            if revision is not None:
                raise ValueError(
                    "Commit {} has multiple git-svn-id trailers".format(hash_bytes.decode('ascii')),
                )
            revision = parsed_rev

        if revision is None:
            return
        mapping.setdefault(revision, []).append(sha)
