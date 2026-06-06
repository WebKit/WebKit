# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import contextlib
import mmap
import os
from collections.abc import Iterator, Mapping
from types import TracebackType
from typing import Optional


ENTRY_SIZE: int = 20
ZERO_ENTRY: bytes = b'\x00' * ENTRY_SIZE


class BinaryRevisionMap(Mapping[int, Optional[str]]):
    """Read-only ``Mapping[int, Optional[str]]`` over a packed file of 20-byte
    SHA1s indexed by ``(revision - 1) * 20``.

    The dense view of the file: ``len(m)`` is the total slot count and
    ``iter(m)`` yields every revision from 1 to ``len(m)`` inclusive. A slot
    that is all-zero (``ZERO_ENTRY``) maps to ``None`` and represents a
    revision the build script flagged as ambiguous (0 or >=2 git commits
    sharing the SVN revision); a populated slot maps to its hex SHA1.

    Out-of-range revisions raise ``KeyError``: ``m[N]`` for ``N`` past
    ``len(m)`` or below 1, and any lookup against a missing or empty file.

    Conforms to the ``Mapping`` protocol: ``__contains__``, ``__eq__``,
    ``keys``, ``items``, ``values``, and ``get`` come from the ABC mixin. The
    inherited ``__eq__`` builds two ~30 MB dicts before comparing, which is
    no good for repeated equality checks — but we don't currently call it
    anywhere, so leaving it as the slow default avoids dragging in a custom
    fast path (path-equality has an ``os.replace`` race; mmap byte-compare
    can stall on demand-paging from disk; ``fstat`` matching needs us to
    retain the file handle just for that purpose). Add a fast path here if
    a real caller appears.

    Lifecycle is the holder's responsibility: either use as a context manager
    via ``with BinaryRevisionMap(path) as m:`` or call ``close()`` explicitly.
    There is intentionally no ``__del__``.
    """

    def __init__(self, path: str) -> None:
        self._stack: contextlib.ExitStack = contextlib.ExitStack()
        self._mmap: Optional[mmap.mmap] = None
        self._view: Optional[memoryview] = None
        self._hash_to_revision: Optional[dict[str, int]] = None
        self.path: str = path

    def _open(self) -> bool:
        """Open and memory-map the backing file if not already open.

        Returns True if a mapping is available, False if the file does not
        exist or is empty (zero-byte). Any other failure (including mmap
        failure on a non-empty file) propagates after the underlying file
        handle is closed so no half-opened state leaks.
        """
        if self._mmap is not None:
            return True
        try:
            f = self._stack.enter_context(open(self.path, 'rb'))
        except FileNotFoundError:
            return False
        # mmap raises ValueError on zero-byte files; treat that the same as a
        # missing file so callers see a uniform "no entries" state.
        if os.fstat(f.fileno()).st_size == 0:
            self._stack.close()
            self._stack = contextlib.ExitStack()
            return False
        try:
            mm = self._stack.enter_context(
                mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ),
            )
        except BaseException:
            self._stack.close()
            self._stack = contextlib.ExitStack()
            raise
        self._mmap = mm
        self._view = memoryview(mm)
        return True

    def close(self) -> None:
        """Release the mmap and file handle. Idempotent; the instance is
        reusable after close (a subsequent lookup re-opens the file)."""
        if self._view is not None:
            self._view.release()
            self._view = None
        # ExitStack runs callbacks in reverse, so the mmap is closed before
        # the underlying file (required on Windows).
        self._mmap = None
        self._stack.close()
        self._stack = contextlib.ExitStack()
        self._hash_to_revision = None

    def __enter__(self) -> 'BinaryRevisionMap':
        return self

    def __exit__(
        self,
        _exc_type: Optional[type[BaseException]],
        _exc: Optional[BaseException],
        _tb: Optional[TracebackType],
    ) -> None:
        self.close()

    def __getitem__(self, revision: int) -> Optional[str]:
        """Return the hex SHA1 for ``revision``, or ``None`` if the slot is
        explicitly null (revision is known-ambiguous: 0 or >=2 commits share
        it). Raises ``KeyError`` if the revision is out of range or the
        backing file is missing or empty. Raises ``OSError`` or ``ValueError``
        on a present-but-unreadable file.
        """
        if not self._open():
            raise KeyError(revision)
        assert self._view is not None  # _open() returned True
        if revision < 1:
            raise KeyError(revision)
        offset = (revision - 1) * ENTRY_SIZE
        if offset + ENTRY_SIZE > len(self._view):
            raise KeyError(revision)
        slot = self._view[offset:offset + ENTRY_SIZE]
        if slot == ZERO_ENTRY:
            return None
        return slot.hex()

    def __iter__(self) -> Iterator[int]:
        """Yield every revision from 1 to ``len(self)`` inclusive (matching
        the dense view of the backing file).
        """
        return iter(range(1, len(self) + 1))

    def __len__(self) -> int:
        """Return the total slot count of the backing file (zero if missing).

        Raises ``OSError`` or ``ValueError`` on a present-but-unreadable file.
        """
        if not self._open():
            return 0
        assert self._view is not None
        return len(self._view) // ENTRY_SIZE

    def hash_to_revision(self) -> dict[str, int]:
        """Return a memoized ``{hex_hash: revision}`` dict for every populated slot.

        Returns an empty (uncached) dict if the backing file is missing, so a
        later call can rebuild if the file appears. Raises ``OSError`` or
        ``ValueError`` on a present-but-unreadable file.
        """
        if self._hash_to_revision is not None:
            return self._hash_to_revision
        if not self._open():
            return {}
        assert self._view is not None

        result: dict[str, int] = {}
        count = len(self._view) // ENTRY_SIZE
        for i in range(count):
            offset = i * ENTRY_SIZE
            slot = self._view[offset:offset + ENTRY_SIZE]
            if slot == ZERO_ENTRY:
                continue
            result[slot.hex()] = i + 1

        self._hash_to_revision = result
        return result
