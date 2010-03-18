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
import urllib
import urllib2
import xmlrpclib

# Import WebKit-specific modules.
from webkitpy.webkit_logging import log

# WebKit includes a built copy of BeautifulSoup in Scripts/webkitpy/thirdparty
# so this import should always succeed.
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup


class Builder(object):
    def __init__(self, name, buildbot):
        self._name = unicode(name)
        self._buildbot = buildbot
        self._builds_cache = {}

    def name(self):
        return self._name

    def url(self):
        return "http://%s/builders/%s" % (self._buildbot.buildbot_host, urllib.quote(self._name))

    def build(self, build_number):
        cached_build = self._builds_cache.get(build_number)
        if cached_build:
            return cached_build

        build_dictionary = self._buildbot._fetch_xmlrpc_build_dictionary(self, build_number)
        if not build_dictionary:
            return None
        build = Build(build_dictionary, self)
        self._builds_cache[build_number] = build
        return build


class Build(object):
    def __init__(self, build_dictionary, builder):
        self._builder = builder
        # FIXME: Knowledge of the XMLRPC specifics should probably not go here.
        self._revision = int(build_dictionary['revision'])
        self._number = int(build_dictionary['number'])
        self._is_green = (build_dictionary['results'] == 0) # Undocumented, buildbot XMLRPC

    @staticmethod
    def build_url(builder, build_number):
        return "%s/builds/%s" % (builder.url(), build_number)

    def url(self):
        return self.build_url(self.builder(), self._number)

    def builder(self):
        return self._builder

    def revision(self):
        return self._revision

    def is_green(self):
        return self._is_green

    def previous_build(self):
        # previous_build() allows callers to avoid assuming build numbers are sequential.
        # They may not be sequential across all master changes, or when non-trunk builds are made.
        return self._builder.build(self._number - 1)


class BuildBot(object):

    default_host = "build.webkit.org"

    def __init__(self, host=default_host):
        self.buildbot_host = host

        # If any Leopard builder/tester, Windows builder or Chromium builder is
        # red we should not be landing patches.  Other builders should be added
        # to this list once they are known to be reliable.
        # See https://bugs.webkit.org/show_bug.cgi?id=33296 and related bugs.
        self.core_builder_names_regexps = [
            "Leopard",
            "Windows.*Build",
            "Chromium",
        ]

    # FIXME: This should create and return Buidler and Build objects instead
    # of a custom dictionary.
    def _parse_builder_status_from_row(self, status_row):
        # If WebKit's buildbot has an XMLRPC interface we could use, we could
        # do something more sophisticated here.  For now we just parse out the
        # basics, enough to support basic questions like "is the tree green?"
        status_cells = status_row.findAll('td')
        builder = {}

        name_link = status_cells[0].find('a')
        builder['name'] = name_link.string

        status_link = status_cells[1].find('a')
        if not status_link:
            # We failed to find a link in the first cell, just give up.  This
            # can happen if a builder is just-added, the first cell will just
            # be "no build"
            # Other parts of the code depend on is_green being present.
            builder['is_green'] = False
            return builder
        # Will be either a revision number or a build number
        revision_string = status_link.string
        # If revision_string has non-digits assume it's not a revision number.
        builder['built_revision'] = int(revision_string) \
                                    if not re.match('\D', revision_string) \
                                    else None
        builder['is_green'] = not re.search('fail',
                                            status_cells[1].renderContents())

        status_link_regexp = r"builders/(?P<builder_name>.*)/builds/(?P<build_number>\d+)"
        link_match = re.match(status_link_regexp, status_link['href'])
        builder['build_number'] = int(link_match.group("build_number"))

        # We could parse out the current activity too.
        return builder

    def _builder_statuses_with_names_matching_regexps(self,
                                                      builder_statuses,
                                                      name_regexps):
        builders = []
        for builder in builder_statuses:
            for name_regexp in name_regexps:
                if re.match(name_regexp, builder['name']):
                    builders.append(builder)
        return builders

    def red_core_builders(self):
        red_builders = []
        for builder in self._builder_statuses_with_names_matching_regexps(
                               self.builder_statuses(),
                               self.core_builder_names_regexps):
            if not builder['is_green']:
                red_builders.append(builder)
        return red_builders

    def red_core_builders_names(self):
        red_builders = self.red_core_builders()
        return map(lambda builder: builder['name'], red_builders)

    def core_builders_are_green(self):
        return not self.red_core_builders()

    # FIXME: These _fetch methods should move to a networking class.
    def _fetch_xmlrpc_build_dictionary(self, builder, build_number):
        # The buildbot XMLRPC API is super-limited.
        # For one, you cannot fetch info on builds which are incomplete.
        proxy = xmlrpclib.ServerProxy("http://%s/xmlrpc" % self.buildbot_host)
        try:
            return proxy.getBuild(builder.name(), int(build_number))
        except xmlrpclib.Fault, err:
            build_url = Build.build_url(builder, build_number)
            log("Error fetching data for %s build %s (%s)" % (builder.name(), build_number, build_url))
            return None

    def _fetch_one_box_per_builder(self):
        build_status_url = "http://%s/one_box_per_builder" % self.buildbot_host
        return urllib2.urlopen(build_status_url)

    def builder_statuses(self):
        builders = []
        soup = BeautifulSoup(self._fetch_one_box_per_builder())
        status_table = soup.find('table')
        for status_row in status_table.findAll('tr'):
            builder = self._parse_builder_status_from_row(status_row)
            builders.append(builder)
        return builders

    def builder_with_name(self, name):
        return Builder(name, self)
