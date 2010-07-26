# Copyright (C) 2010, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""GDB support for WebKit WTF types.

Add this to your gdb by amending your ~/.gdbinit as follows:
  python
  import sys
  sys.path.insert(0, "/path/to/tools/gdb/")
  import wtf

See http://sourceware.org/gdb/current/onlinedocs/gdb/Python.html for GDB's
Python API.
"""

import gdb
import re


class WTFVectorPrinter:
    """Pretty Printer for a WTF::Vector.

    The output of this pretty printer is similar to the output of std::vector's
    pretty printer, which is bundled in gcc.

    Example gdb session should look like:
    (gdb) p v
    $3 = WTF::Vector of length 7, capacity 16 = {7, 17, 27, 37, 47, 57, 67}
    (gdb) set print elements 3
    (gdb) p v
    $6 = WTF::Vector of length 7, capacity 16 = {7, 17, 27...}
    (gdb) set print array
    (gdb) p v
    $7 = WTF::Vector of length 7, capacity 16 = {
      7,
      17,
      27
      ...
    }
    (gdb) set print elements 200
    (gdb) p v
    $8 = WTF::Vector of length 7, capacity 16 = {
      7,
      17,
      27,
      37,
      47,
      57,
      67
    }
    """

    class Iterator:
        def __init__(self, start, finish):
            self.item = start
            self.finish = finish
            self.count = 0

        def __iter__(self):
            return self

        def next(self):
            if self.item == self.finish:
                raise StopIteration
            count = self.count
            self.count += 1
            element = self.item.dereference()
            self.item += 1
            return ('[%d]' % count, element)

    def __init__(self, val):
        self.val = val

    def children(self):
        start = self.val['m_buffer']['m_buffer']
        return self.Iterator(start, start + self.val['m_size'])

    def to_string(self):
        return ('%s of length %d, capacity %d'
                % ('WTF::Vector', self.val['m_size'], self.val['m_buffer']['m_capacity']))

    def display_hint(self):
        return 'array'


def lookup_function(val):
    type = val.type
    if type.code == gdb.TYPE_CODE_REF:
        type = type.target()
    type = type.unqualified().strip_typedefs()
    typename = type.tag
    if not typename:
        return None
    for function, pretty_printer in pretty_printers_dict.items():
        if function.search(typename):
            return pretty_printer(val)
    return None


def build_pretty_printers_dict():
    pretty_printers_dict[re.compile('^WTF::Vector<.*>$')] = WTFVectorPrinter


pretty_printers_dict = {}

build_pretty_printers_dict()

gdb.pretty_printers.append(lookup_function)
