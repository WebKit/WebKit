# Copyright (C) 2013 Samsung Electronics. All rights reserved.
#
# Based on code from Chromium, copyright as follows:
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
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

from collections import defaultdict
import hashlib
import logging
import re
import subprocess
from xml.dom.minidom import parseString
from xml.parsers.expat import ExpatError

_log = logging.getLogger(__name__)


def get_text_of(top_node, name):
    """ Returns all text in all DOM nodes with a certain |name| that are children of |top_node|. """

    text = ""
    for nodes_named in top_node.getElementsByTagName(name):
        text += "".join([node.data for node in nodes_named.childNodes
                         if node.nodeType == node.TEXT_NODE])
    return text


def get_CDATA_of(top_node, name):
    """ Returns all CDATA in all DOM nodes with a certain |name| that are children of |top_node|. """

    text = ""
    for nodes_named in top_node.getElementsByTagName(name):
        text += "".join([node.data for node in nodes_named.childNodes
                         if node.nodeType == node.CDATA_SECTION_NODE])
    if (text == ""):
        return None
    return text


# Constants that give real names to the abbreviations in valgrind XML output.
INSTRUCTION_POINTER = "ip"
OBJECT_FILE = "obj"
FUNCTION_NAME = "fn"
SRC_FILE_DIR = "dir"
SRC_FILE_NAME = "file"
SRC_LINE = "line"


def gather_frames(node, source_dir):
    frame_dict = lambda frame: {
        INSTRUCTION_POINTER: get_text_of(frame, INSTRUCTION_POINTER),
        OBJECT_FILE: get_text_of(frame, OBJECT_FILE),
        FUNCTION_NAME: get_text_of(frame, FUNCTION_NAME),
        SRC_FILE_DIR: get_text_of(frame, SRC_FILE_DIR),
        SRC_FILE_NAME: get_text_of(frame, SRC_FILE_NAME),
        SRC_LINE: get_text_of(frame, SRC_LINE)}

    return [frame_dict(frame) for frame in node.getElementsByTagName("frame")]


class ValgrindError:

    def __init__(self, executive, source_dir, error_node):
        self._executive = executive
        self._kind = get_text_of(error_node, "kind")
        self._backtraces = []
        self._suppression = None
        self._additional = []

        # Iterate through the nodes, parsing <what|auxwhat><stack> pairs.
        description = None
        for node in error_node.childNodes:
            if node.localName == "what" or node.localName == "auxwhat":
                description = "".join([n.data for n in node.childNodes
                                       if n.nodeType == n.TEXT_NODE])
            elif node.localName == "xwhat":
                description = get_text_of(node, "text")
            elif node.localName == "stack":
                assert description
                self._backtraces.append([description, gather_frames(node, source_dir)])
                description = None
            elif node.localName == "origin":
                description = get_text_of(node, "what")
                stack = node.getElementsByTagName("stack")[0]
                frames = gather_frames(stack, source_dir)
                self._backtraces.append([description, frames])
                description = None
                stack = None
                frames = None
            elif description and node.localName != None:
                # The lastest description has no stack, e.g. "Address 0x28 is unknown".
                self._additional.append(description)
                description = None

            if node.localName == "suppression":
                self._suppression = get_CDATA_of(node, "rawtext")

    def __str__(self):
        output = self._kind + "\n"
        for backtrace in self._backtraces:
            output += backtrace[0] + "\n"

            buf = ""
            for frame in backtrace[1]:
                buf += (frame[FUNCTION_NAME] or frame[INSTRUCTION_POINTER]) + "\n"

            input = buf.encode('latin-1').split("\n")
            demangled_names = [self._executive.run_command(['c++filt', '-n', name]) for name in input if name]

            i = 0
            for frame in backtrace[1]:
                output += ("  " + demangled_names[i])
                i = i + 1

                if frame[SRC_FILE_DIR] != "":
                    output += (" (" + frame[SRC_FILE_DIR] + "/" + frame[SRC_FILE_NAME] +
                               ":" + frame[SRC_LINE] + ")")
                else:
                    output += " (" + frame[OBJECT_FILE] + ")"
                output += "\n"

        for additional in self._additional:
            output += additional + "\n"

        assert self._suppression != None, "Your Valgrind doesn't generate " \
                                           "suppressions - is it too old?"

        output += "Suppression (error hash=#%016X#):\n" % self.error_hash()

        # Widen the suppressions slightly.
        supp = self._suppression
        supp = supp.replace("fun:_Znwj", "fun:_Znw*")
        supp = supp.replace("fun:_Znwm", "fun:_Znw*")
        supp = supp.replace("fun:_Znaj", "fun:_Zna*")
        supp = supp.replace("fun:_Znam", "fun:_Zna*")

        # Split into lines so we can enforce length limits.
        supplines = supp.split("\n")
        supp = None  # to avoid re-use

        # Truncate at line 26 (VG_MAX_SUPP_CALLERS plus 2 for name and type)
        # (https://bugs.kde.org/show_bug.cgi?id=199468 proposes raising
        # VG_MAX_SUPP_CALLERS, but we're probably fine with it as is.)
        newlen = min(26, len(supplines))

        if (len(supplines) > newlen):
            supplines = supplines[0:newlen]
            supplines.append("}")

        for frame in range(len(supplines)):
            # Replace the always-changing anonymous namespace prefix with "*".
            m = re.match("( +fun:)_ZN.*_GLOBAL__N_.*\.cc_" +
                         "[0-9a-fA-F]{8}_[0-9a-fA-F]{8}(.*)",
                         supplines[frame])
            if m:
                supplines[frame] = "*".join(m.groups())

        return output + "\n".join(supplines) + "\n"

    def unique_string(self):
        rep = self._kind + " "
        for backtrace in self._backtraces:
            for frame in backtrace[1]:
                rep += frame[FUNCTION_NAME]

                if frame[SRC_FILE_DIR] != "":
                    rep += frame[SRC_FILE_DIR] + "/" + frame[SRC_FILE_NAME]
                else:
                    rep += frame[OBJECT_FILE]
        return rep

    def error_hash(self):
        # This is a device-independent hash identifying the suppression.
        # By printing out this hash we can find duplicate reports between tests and
        # different shards running on multiple buildbots
        return int(hashlib.md5(self.unique_string()).hexdigest()[:16], 16)

    def __hash__(self):
        return hash(self.unique_string())

    def __eq__(self, rhs):
        return self.unique_string() == rhs


class LeakDetectorValgrind(object):

    def __init__(self, executive, filesystem, source_dir):
        self._executive = executive
        self._filesystem = filesystem
        self._source_dir = source_dir

        # Contains the set of unique errors.
        self._errors = set()
        # Contains all suppressions used.
        self._suppressions = defaultdict(int)

    def _parse_leaks_output(self, leaks_output):
        try:
            parsed_string = parseString(leaks_output)
        except ExpatError as e:
            parse_failed = True
            _log.error("could not parse %s: %s" % (leaks_output, e))
            return

        cur_report_errors = set()

        commandline = None
        preamble = parsed_string.getElementsByTagName("preamble")[0]
        for node in preamble.getElementsByTagName("line"):
            if node.localName == "line":
                for x in node.childNodes:
                    if x.nodeType == node.TEXT_NODE and "Command" in x.data:
                        commandline = x.data
                        break

        raw_errors = parsed_string.getElementsByTagName("error")
        for raw_error in raw_errors:
            # Ignore "possible" leaks and InvalidRead/Write by default.
            if (get_text_of(raw_error, "kind") != "Leak_PossiblyLost") and \
                (get_text_of(raw_error, "kind") != "Leak_StillReachable") and \
                (get_text_of(raw_error, "kind") != "InvalidWrite") and \
                (get_text_of(raw_error, "kind") != "InvalidRead"):
                error = ValgrindError(self._executive, self._source_dir, raw_error)
                if error not in cur_report_errors:
                    # We haven't seen such errors doing this report yet...
                    if error in self._errors:
                        # ... but we saw it in earlier reports, e.g. previous UI test
                        cur_report_errors.add("This error was already printed in "
                                              "some other test, see 'hash=#%016X#'" % \
                            error.error_hash())
                    else:
                        # ... and we haven't seen it in other tests as well
                        self._errors.add(error)
                        cur_report_errors.add(error)

        suppcountlist = parsed_string.getElementsByTagName("suppcounts")
        if len(suppcountlist) > 0:
            suppcountlist = suppcountlist[0]
            for node in suppcountlist.getElementsByTagName("pair"):
                count = get_text_of(node, "count")
                name = get_text_of(node, "name")
                self._suppressions[name] += int(count)

        return cur_report_errors

    def leaks_files_in_results_directory(self):
        return self._filesystem.glob(self._filesystem.join(self._source_dir, "drt-*-leaks.xml"))

    def clean_leaks_files_from_results_directory(self):
        # Remove old Valgrind xml files before starting this run.
        leaks_files = self.leaks_files_in_results_directory()
        for f in leaks_files:
            self._filesystem.remove(f)

    def parse_and_print_leaks_detail(self, leaks_files):
        for f in leaks_files:
            leaks_output = self._filesystem.read_binary_file(f)
            detected_leaks = self._parse_leaks_output(leaks_output)

        _log.info("-----------------------------------------------------")
        _log.info("Suppressions used:")
        _log.info("  count name")
        for (name, count) in sorted(self._suppressions.items(), key=lambda (k, v): (v, k)):
            _log.info("%7d %s" % (count, name))
        _log.info("-----------------------------------------------------")

        if self._errors:
            _log.info("Valgrind detected %s leaks:" % len(self._errors))
            for leak in self._errors:
                _log.info(leak)
