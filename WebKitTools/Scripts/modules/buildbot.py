# Copyright (c) 2009, Google Inc. All rights reserved.
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
#
# WebKit's Python module for interacting with WebKit's buildbot

import re
import urllib2

# Import WebKit-specific modules.
from modules.logging import log

# WebKit includes a built copy of BeautifulSoup in Scripts/modules
# so this import should always succeed.
from .BeautifulSoup import BeautifulSoup

class BuildBot:
    def __init__(self, host="build.webkit.org"):
        self.buildbot_host = host
        self.buildbot_server_url = "http://%s/" % self.buildbot_host
        
        # If any of the Leopard build/test bots or the Windows builders are red we should not be landing patches.
        # Other builders should be added to this list once they're known to be stable.
        self.core_builder_names_regexps = [ 'Leopard', "Windows.*Build" ]

    # If WebKit's buildbot has an XMLRPC interface we could use, we could do something more sophisticated here.
    # For now we just parse out the basics, enough to support basic questions like "is the tree green?"
    def _parse_builder_status_from_row(self, status_row):
        status_cells = status_row.findAll('td')
        builder = {}

        name_link = status_cells[0].find('a')
        builder['name'] = name_link.string
        # We could generate the builder_url from the name in a future version of this code.
        builder['builder_url'] = self.buildbot_server_url + name_link['href']

        status_link = status_cells[1].find('a')
        if not status_link:
            # We failed to find a link in the first cell, just give up.
            # This can happen if a builder is just-added, the first cell will just be "no build"
            builder['is_green'] = False # Other parts of the code depend on is_green being present.
            return builder
        revision_string = status_link.string # Will be either a revision number or a build number
        # If revision_string has non-digits assume it's not a revision number.
        builder['built_revision'] = int(revision_string) if not re.match('\D', revision_string) else None
        builder['is_green'] = not re.search('fail', status_cells[1].renderContents())
        # We could parse out the build number instead, but for now just store the URL.
        builder['build_url'] = self.buildbot_server_url + status_link['href']

        # We could parse out the current activity too.

        return builder

    def _builder_statuses_with_names_matching_regexps(self, builder_statuses, name_regexps):
        builders = []
        for builder in builder_statuses:
            for name_regexp in name_regexps:
                if re.match(name_regexp, builder['name']):
                    builders.append(builder)
        return builders

    def red_core_builders(self):
        red_builders = []
        for builder in self._builder_statuses_with_names_matching_regexps(self.builder_statuses(), self.core_builder_names_regexps):
            if not builder['is_green']:
                red_builders.append(builder)
        return red_builders

    def red_core_builders_names(self):
        red_builders = self.red_core_builders()
        return map(lambda builder: builder['name'], red_builders)

    def core_builders_are_green(self):
        return not self.red_core_builders()

    def builder_statuses(self):
        build_status_url = self.buildbot_server_url + 'one_box_per_builder'
        page = urllib2.urlopen(build_status_url)
        soup = BeautifulSoup(page)

        builders = []
        status_table = soup.find('table')
        for status_row in status_table.findAll('tr'):
            builder = self._parse_builder_status_from_row(status_row)
            builders.append(builder)
        return builders
