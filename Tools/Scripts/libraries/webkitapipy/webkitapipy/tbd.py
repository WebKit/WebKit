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
from typing import NamedTuple, Iterable, Optional
from pathlib import Path
import subprocess
import json


class ExportList(NamedTuple):
    symbols: list[str]
    weak_symbols: list[str]
    objc_classes: list[str]
    targets: Optional[list[str]]

    @classmethod
    def from_dict(cls, doc: dict) -> ExportList:
        data = doc['data']
        return cls(symbols=data.get('global', []),
                   weak_symbols=data.get('weak', []),
                   objc_classes=data.get('objc_class', []),
                   targets=doc.get('targets'))


class TBD(NamedTuple):
    exports: list[ExportList]
    reexports: list[ExportList]
    install_name: str
    targets: list[str]

    @classmethod
    def from_dict(cls, doc: dict) -> Iterable[TBD]:
        for library in (doc['main_library'], *doc.get('libraries', ())):
            exports = library.get('exported_symbols', ())
            reexports = library.get('reexported_symbols', ())
            yield cls(
                exports=list(map(ExportList.from_dict, exports)),
                reexports=list(map(ExportList.from_dict, reexports)),
                install_name=library['install_names'][0]['name'],
                targets=[info['target'] for info in library['target_info']],
            )

    @classmethod
    def from_file(cls, file: Path) -> Iterable[TBD]:
        # In SDKs, .tbds are a YAML-based format (v4) but they can be converted
        # to a JSON-based format (v5) by recent toolchains.
        readtapi = subprocess.run(('xcrun', 'llvm-readtapi',
                                   '--filetype=tbd-v5', file),
                                  check=True, stdout=subprocess.PIPE, text=True)
        doc = json.loads(readtapi.stdout)
        yield from cls.from_dict(doc)
