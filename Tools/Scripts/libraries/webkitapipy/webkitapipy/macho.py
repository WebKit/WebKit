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

import re
import subprocess
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import NamedTuple

objc_fully_qualified_method = re.compile(r'[-+]\[(?P<class>\S+) (?P<selector>[^\]]+)\]')


@dataclass
class APIReport:
    file: Path
    arch: str

    exports: set[str] = field(default_factory=set)
    methods: set[APIReport.Selector] = field(default_factory=set)
    imports: set[str] = field(default_factory=set)
    selrefs: set[str] = field(default_factory=set)

    class Selector(NamedTuple):
        name: str
        class_: str

    @classmethod
    def from_binary(cls, binary_path: Path, *, arch: str, exports_only=False):
        dyld_args = ['-arch', arch, '-exports', '-objc']
        if not exports_only:
            dyld_args.extend(('-imports',
                              # WebKit binaries typically use __DATA_CONST, but
                              # non-system executables may use __DATA.
                              '-section', '__DATA', '__objc_selrefs',
                              '-section', '__DATA_CONST', '__objc_selrefs',
                              '-section_bytes', '__TEXT', '__dlsym_cstr',
                              '-section_bytes', '__TEXT', '__getClass_cstr'))
        dyld = subprocess.run(('xcrun', 'dyld_info', *dyld_args, binary_path),
                              check=True, stdout=subprocess.PIPE, text=True)

        report = cls(file=binary_path, arch=arch)
        report._populate_from_dyld_info(dyld.stdout)
        return report

    def _populate_from_dyld_info(self, dyld_output: str):
        Sect = Enum('Sect', 'EXPORTS IMPORTS OBJC SELREFS DLSYM GETCLASS')
        in_section = None
        next_cstr = bytearray()

        header_line = f'{self.file} [{self.arch}]:'
        for line in dyld_output.splitlines():
            # Each of dyld_info's flags prints its own section of the output.
            # In this line-based parser, keep track of which section we are in,
            # read one line at a time, and update the report as we encounter
            # interesting data.
            line = line.strip()
            if line == header_line:
                continue

            # Detect changes to the section of output.
            if line == '-exports:':
                in_section = Sect.EXPORTS
            elif line == '-imports:':
                in_section = Sect.IMPORTS
            elif line == '-objc:':
                in_section = Sect.OBJC
            elif line in ('(__DATA,__objc_selrefs) section:',
                          '(__DATA_CONST,__objc_selrefs) section:'):
                in_section = Sect.SELREFS
            elif line == '(__TEXT,__dlsym_cstr) section:':
                in_section = Sect.DLSYM
            elif line == '(__TEXT,__getClass_cstr) section:':
                in_section = Sect.GETCLASS

            # Parse symbol information based on the current section.
            elif in_section == Sect.EXPORTS:
                # Address in binary and symbol name:
                # ```
                # 0x00A5B75C  _JSBigIntCreateWithDouble
                # ```
                offset, symbol = line.split(maxsplit=1)
                # skip the header line
                if offset != 'offset':
                    self.exports.add(symbol)
            elif in_section == Sect.IMPORTS:
                # Hexadecimal index, symbol name, optional linkage tag, and
                # containing dylib as recorded in the two-level namespace:
                # ```
                # 0x02E0  _voucher_mach_msg_set [weak-import] (from libSystem)
                # ```
                idx, symbol, metadata = line.split(maxsplit=2)
                dylib = metadata[metadata.index('(from ') + 6:-1]
                if dylib != '<this-image>':
                    self.imports.add(symbol)
            elif in_section == Sect.OBJC:
                # ObjC-like declaration for classes and protocols with method
                # names and method addresses for classes:
                # ```
                # @interface WebEventRegion : NSObject <NSCopying>
                #   0x02134AC4  -[WebEventRegion initWithPoints::::]
                #   0x02134B80  -[WebEventRegion copyWithZone:]
                #   0x02134BB8  -[WebEventRegion description]
                #   0x02134C58  -[WebEventRegion hitTest:]
                #   0x02134CD4  -[WebEventRegion isEqual:]
                #   0x02134E94  -[WebEventRegion quad]
                #   0x02134F74  -[WebEventRegion p1]
                #   0x02134F9C  -[WebEventRegion p2]
                #   0x02134FC4  -[WebEventRegion p3]
                #   0x02134FEC  -[WebEventRegion p4]
                # @end
                # @protocol NSCopying :
                #   -[NSCopying copyWithZone:]
                # @end
                # ```
                m = objc_fully_qualified_method.search(line)
                if m:
                    sel = self.Selector._make(m.group('selector', 'class'))
                    self.methods.add(sel)
                elif '@interface' not in line and \
                        '@protocol' not in line and '@end' not in line:
                    print(f'warning:{self.file} unrecognized '
                          f'dyld_info -objc line: "{line}"', file=sys.stderr)
            elif in_section == Sect.SELREFS:
                # Address in binary of selref data, followed by selector
                # name:
                # ```
                # 0x0A8043D8  "interruption:"
                # ```
                address, quoted_name = line.split(maxsplit=1)
                self.selrefs.add(quoted_name.strip('"'))
            elif in_section in (Sect.DLSYM, Sect.GETCLASS):
                # hexdump-style output, separated by lines containing symbol
                # names:
                # ```
                # __ZZL22initWebFilterEvaluatorvE16auditedClassName:
                # 0x0A26DD50: 57 65 62 46 69 6C 74 65 72 45 76 61 6C 75 61 74
                # 0x0A26DD60: 6F 72 00
                # ```
                address_or_symbol, sep, data = line.partition(': ')
                if sep != ': ':
                    continue
                for char in bytes.fromhex(data):
                    if char != 0:
                        next_cstr.append(char)
                        continue
                    name = next_cstr.decode()
                    if in_section == Sect.GETCLASS:
                        self.imports.add(f'_OBJC_CLASS_$_{name}')
                    else:
                        self.imports.add(f'_{name}')
                    del next_cstr[:]
            else:
                print(f'warning:{self.file}: unrecognized '
                      f'dyld_info line: "{line}"', file=sys.stderr)
