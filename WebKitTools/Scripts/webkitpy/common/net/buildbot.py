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

import operator
import re
import urllib
import urllib2
import xmlrpclib

# Import WebKit-specific modules.
from webkitpy.common.system.deprecated_logging import log

# WebKit includes a built copy of BeautifulSoup in Scripts/webkitpy/thirdparty
# so this import should always succeed.
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup


class Builder(object):
    def __init__(self, name, buildbot):
        self._name = unicode(name)
        self._buildbot = buildbot
        self._builds_cache = {}
        self._revision_to_build_number = None

    def name(self):
        return self._name

    def results_url(self):
        return "http://%s/results/%s" % (self._buildbot.buildbot_host, self.url_encoded_name())

    def url_encoded_name(self):
        return urllib.quote(self._name)

    def url(self):
        return "http://%s/builders/%s" % (self._buildbot.buildbot_host, self.url_encoded_name())

    # This provides a single place to mock
    def _fetch_build(self, build_number):
        build_dictionary = self._buildbot._fetch_xmlrpc_build_dictionary(self, build_number)
        if not build_dictionary:
            return None
        return Build(self,
            build_number=int(build_dictionary['number']),
            revision=int(build_dictionary['revision']),
            is_green=(build_dictionary['results'] == 0) # Undocumented, buildbot XMLRPC, 0 seems to mean "pass"
        )

    def build(self, build_number):
        if not build_number:
            return None
        cached_build = self._builds_cache.get(build_number)
        if cached_build:
            return cached_build

        build = self._fetch_build(build_number)
        self._builds_cache[build_number] = build
        return build

    file_name_regexp = re.compile(r"r(?P<revision>\d+) \((?P<build_number>\d+)\)")
    def _revision_and_build_for_filename(self, filename):
        # Example: "r47483 (1)/" or "r47483 (1).zip"
        match = self.file_name_regexp.match(filename)
        return (int(match.group("revision")), int(match.group("build_number")))

    def _fetch_revision_to_build_map(self):
        # All _fetch requests go through _buildbot for easier mocking
        try:
            # FIXME: This method is horribly slow due to the huge network load.
            # FIXME: This is a poor way to do revision -> build mapping.
            # Better would be to ask buildbot through some sort of API.
            result_files = self._buildbot._fetch_twisted_directory_listing(self.results_url())
        except urllib2.HTTPError, error:
            if error.code != 404:
                raise
            result_files = []

        # This assumes there was only one build per revision, which is false but we don't care for now.
        return dict([self._revision_and_build_for_filename(file_info["filename"]) for file_info in result_files])

    # This assumes there can be only one build per revision, which is false, but we don't care for now.
    def build_for_revision(self, revision, allow_failed_lookups=False):
        if not self._revision_to_build_number:
            self._revision_to_build_number = self._fetch_revision_to_build_map()
        # NOTE: This lookup will fail if that exact revision was never built.
        build_number = self._revision_to_build_number.get(int(revision))
        build = self.build(build_number)
        if not build and allow_failed_lookups:
            # Builds for old revisions with fail to lookup via buildbot's xmlrpc api.
            build = Build(self,
                build_number=build_number,
                revision=revision,
                is_green=False,
            )
        return build

    # FIXME: We should not have to pass a red_build_number, but rather Builder should
    # know what its "latest_build" is.
    def find_green_to_red_transition(self, red_build_number, look_back_limit=30):
        # walk backwards until we find a green build
        red_build = self.build(red_build_number)
        green_build = None
        look_back_count = 0
        while red_build:
            if look_back_count >= look_back_limit:
                break
            # Use a previous_build() method to avoid assuming build numbers are sequential.
            before_red_build = red_build.previous_build()
            if before_red_build and before_red_build.is_green():
                green_build = before_red_build
                break
            red_build = before_red_build
            look_back_count += 1
        return (green_build, red_build)

    def suspect_revisions_for_green_to_red_transition(self, red_build_number, look_back_limit=30, avoid_flakey_tests=True):
        (last_green_build, first_red_build) = self.find_green_to_red_transition(red_build_number, look_back_limit)
        if not last_green_build:
            return [] # We ran off the limit of our search
        # if avoid_flakey_tests, require at least 2 red builds before we suspect a green to red transition.
        if avoid_flakey_tests and first_red_build == self.build(red_build_number):
            return []
        suspect_revisions = range(first_red_build.revision(), last_green_build.revision(), -1)
        suspect_revisions.reverse()
        return suspect_revisions


class LayoutTestResults(object):
    @classmethod
    def _parse_results_html(cls, page):
        parsed_results = {}
        tables = BeautifulSoup(page).findAll("table")
        for table in tables:
            table_title = table.findPreviousSibling("p").string
            # We might want to translate table titles into identifiers at some point.
            parsed_results[table_title] = [row.find("a").string for row in table.findAll("tr")]

        return parsed_results

    @classmethod
    def _fetch_results_html(cls, base_url):
        results_html = "%s/results.html" % base_url
        # FIXME: We need to move this sort of 404 logic into NetworkTransaction or similar.
        try:
            page = urllib2.urlopen(results_html)
            return cls._parse_results_html(page)
        except urllib2.HTTPError, error:
            if error.code != 404:
                raise

    @classmethod
    def results_from_url(cls, base_url):
        parsed_results = cls._fetch_results_html(base_url)
        if not parsed_results:
            return None
        return cls(base_url, parsed_results)

    def __init__(self, base_url, parsed_results):
        self._base_url = base_url
        self._parsed_results = parsed_results

    def parsed_results(self):
        return self._parsed_results


class Build(object):
    def __init__(self, builder, build_number, revision, is_green):
        self._builder = builder
        self._number = build_number
        self._revision = revision
        self._is_green = is_green
        self._layout_test_results = None

    @staticmethod
    def build_url(builder, build_number):
        return "%s/builds/%s" % (builder.url(), build_number)

    def url(self):
        return self.build_url(self.builder(), self._number)

    def results_url(self):
        results_directory = "r%s (%s)" % (self.revision(), self._number)
        return "%s/%s" % (self._builder.results_url(), urllib.quote(results_directory))

    def layout_test_results(self):
        if not self._layout_test_results:
            self._layout_test_results = LayoutTestResults.results_from_url(self.results_url())
        return self._layout_test_results

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
    # FIXME: This should move into some sort of webkit_config.py
    default_host = "build.webkit.org"

    def __init__(self, host=default_host):
        self.buildbot_host = host
        self._builder_by_name = {}

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

    def _matches_regexps(self, builder_name, name_regexps):
        for name_regexp in name_regexps:
            if re.match(name_regexp, builder_name):
                return True
        return False

    # FIXME: Should move onto Builder
    def _is_core_builder(self, builder_name):
        return self._matches_regexps(builder_name, self.core_builder_names_regexps)

    # FIXME: This method needs to die, but is used by a unit test at the moment.
    def _builder_statuses_with_names_matching_regexps(self, builder_statuses, name_regexps):
        return [builder for builder in builder_statuses if self._matches_regexps(builder["name"], name_regexps)]

    def red_core_builders(self):
        return [builder for builder in self.core_builder_statuses() if not builder["is_green"]]

    def red_core_builders_names(self):
        return [builder["name"] for builder in self.red_core_builders()]

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

    def _parse_twisted_file_row(self, file_row):
        string_or_empty = lambda string: str(string) if string else ""
        file_cells = file_row.findAll('td')
        return {
            "filename" : string_or_empty(file_cells[0].find("a").string),
            "size" : string_or_empty(file_cells[1].string),
            "type" : string_or_empty(file_cells[2].string),
            "encoding" : string_or_empty(file_cells[3].string),
        }

    def _parse_twisted_directory_listing(self, page):
        soup = BeautifulSoup(page)
        # HACK: Match only table rows with a class to ignore twisted header/footer rows.
        file_rows = soup.find('table').findAll('tr', { "class" : True })
        return [self._parse_twisted_file_row(file_row) for file_row in file_rows]

    # FIXME: There should be a better way to get this information directly from twisted.
    def _fetch_twisted_directory_listing(self, url):
        return self._parse_twisted_directory_listing(urllib2.urlopen(url))

    def builders(self):
        return [self.builder_with_name(status["name"]) for status in self.builder_statuses()]

    def builder_statuses(self):
        soup = BeautifulSoup(self._fetch_one_box_per_builder())
        return [self._parse_builder_status_from_row(status_row) for status_row in soup.find('table').findAll('tr')]

    def core_builder_statuses(self):
        return [builder for builder in self.builder_statuses() if self._is_core_builder(builder["name"])]

    def builder_with_name(self, name):
        builder = self._builder_by_name.get(name)
        if not builder:
            builder = Builder(name, self)
            self._builder_by_name[name] = builder
        return builder

    def revisions_causing_failures(self, only_core_builders=True):
        builder_statuses = self.core_builder_statuses() if only_core_builders else self.builder_statuses()
        revision_to_failing_bots = {}
        for builder_status in builder_statuses:
            if builder_status["is_green"]:
                continue
            builder = self.builder_with_name(builder_status["name"])
            revisions = builder.suspect_revisions_for_green_to_red_transition(builder_status["build_number"])
            for revision in revisions:
                failing_bots = revision_to_failing_bots.get(revision, [])
                failing_bots.append(builder)
                revision_to_failing_bots[revision] = failing_bots
        return revision_to_failing_bots

    # FIXME: This is a hack around lack of Builder.latest_build() support
    def _latest_builds_from_builders(self, only_core_builders=True):
        builder_statuses = self.core_builder_statuses() if only_core_builders else self.builder_statuses()
        return [self.builder_with_name(status["name"]).build(status["build_number"]) for status in builder_statuses]

    def _build_at_or_before_revision(self, build, revision):
        while build:
            if build.revision() <= revision:
                return build
            build = build.previous_build()

    def last_green_revision(self, only_core_builders=True):
        builds = self._latest_builds_from_builders(only_core_builders)
        target_revision = builds[0].revision()
        # An alternate way to do this would be to start at one revision and walk backwards
        # checking builder.build_for_revision, however build_for_revision is very slow on first load.
        while True:
            # Make builds agree on revision
            builds = [self._build_at_or_before_revision(build, target_revision) for build in builds]
            if None in builds: # One of the builds failed to load from the server.
                return None
            min_revision = min(map(lambda build: build.revision(), builds))
            if min_revision != target_revision:
                target_revision = min_revision
                continue # Builds don't all agree on revision, keep searching
            # Check to make sure they're all green
            all_are_green = reduce(operator.and_, map(lambda build: build.is_green(), builds))
            if not all_are_green:
                target_revision -= 1
                continue
            return min_revision
