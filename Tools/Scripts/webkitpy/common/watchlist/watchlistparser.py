# Copyright (C) 2011 Google Inc. All rights reserved.
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

import re
from webkitpy.common.watchlist.watchlist import WatchList


class WatchListParser(object):
    _DEFINITIONS = 'DEFINITIONS'
    _INVALID_DEFINITION_NAME_REGEX = r'\|'

    def __init__(self):
        self._section_parsers = {self._DEFINITIONS: self._parse_definition_section, }
        self._definition_pattern_parsers = {}

    def parse(self, watch_list_contents):
        watch_list = WatchList()

        # Change the watch list text into a dictionary.
        dictionary = self._eval_watch_list(watch_list_contents)

        # Parse the top level sections in the watch list.
        for section in dictionary:
            parser = self._section_parsers.get(section)
            if not parser:
                raise Exception('Unknown section "%s" in watch list.' % section)
            parser(dictionary[section], watch_list)

        return watch_list

    def _eval_watch_list(self, watch_list_contents):
        return eval(watch_list_contents, {'__builtins__': None}, None)

    def _parse_definition_section(self, definition_section, watch_list):
        definitions = {}
        for name in definition_section:
            invalid_character = re.search(self._INVALID_DEFINITION_NAME_REGEX, name)
            if invalid_character:
                raise Exception('Invalid character "%s" in definition "%s".' % (invalid_character.group(0), name))

            definition = definition_section[name]
            definitions[name] = []
            for pattern_type in definition:
                pattern_parser = self._definition_pattern_parsers.get(pattern_type)
                if not pattern_parser:
                    raise Exception('Invalid pattern type "%s" in definition "%s".' % (pattern_type, name))

                pattern = pattern_parser(definition[pattern_type])
                definitions[name].append(pattern)
        watch_list.set_definitions(definitions)
