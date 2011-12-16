# Copyright (c) 2010 Google Inc. All rights reserved.
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

import platform
import re


# We use this instead of calls to platform directly to allow mocking.
class PlatformInfo(object):
    def __init__(self, executive):
        self._executive = executive

    def display_name(self):
        # platform.platform() returns Darwin information for Mac, which is just confusing.
        if platform.system() == "Darwin":
            return "Mac OS X %s" % platform.mac_ver()[0]

        # Returns strings like:
        # Linux-2.6.18-194.3.1.el5-i686-with-redhat-5.5-Final
        # Windows-2008ServerR2-6.1.7600
        return platform.platform()

    def total_bytes_memory(self):
        system_name = platform.system()
        if system_name == "Darwin":
            return int(self._executive.run_command(["sysctl", "-n", "hw.memsize"]))
        return None

    def _compute_bytes_from_vm_stat_output(self, label_text, vm_stat_output):
        page_size_match = re.search(r"page size of (\d+) bytes", vm_stat_output)
        free_pages_match = re.search(r"%s:\s+(\d+)." % label_text, vm_stat_output)
        if not page_size_match or not free_pages_match:
            return None
        free_page_count = int(free_pages_match.group(1))
        page_size = int(page_size_match.group(1))
        return free_page_count * page_size

    def free_bytes_memory(self):
        system_name = platform.system()
        if system_name == "Darwin":
            vm_stat_output = self._executive.run_command(["vm_stat"])
            free_bytes = self._compute_bytes_from_vm_stat_output("Pages free", vm_stat_output)
            # Per https://bugs.webkit.org/show_bug.cgi?id=74650 include inactive memory since the OS is lazy about freeing memory.
            free_bytes += self._compute_bytes_from_vm_stat_output("Pages inactive", vm_stat_output)
            return free_bytes
        return None
