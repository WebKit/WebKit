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

from webkitpy.common.net.failuremap import FailureMap
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.regressionwindow import RegressionWindow
from webkitpy.common.system.logutils import get_logger
from webkitpy.thirdparty.autoinstalled.mechanize import Browser
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup

_log = get_logger(__file__)


class Builder(object):
    def __init__(self, name, buildbot):
        self._name = name
        self._buildbot = buildbot
        self._builds_cache = {}
        self._revision_to_build_number = None
        self._browser = Browser()
        self._browser.set_handle_robots(False) # The builder pages are excluded by robots.txt

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

    def force_build(self, username="webkit-patch", comments=None):
        def predicate(form):
            try:
                return form.find_control("username")
            except Exception, e:
                return False
        self._browser.open(self.url())
        self._browser.select_form(predicate=predicate)
        self._browser["username"] = username
        if comments:
            self._browser["comments"] = comments
        return self._browser.submit()

    file_name_regexp = re.compile(r"r(?P<revision>\d+) \((?P<build_number>\d+)\)")
    def _revision_and_build_for_filename(self, filename):
        # Example: "r47483 (1)/" or "r47483 (1).zip"
        match = self.file_name_regexp.match(filename)
        return (int(match.group("revision")), int(match.group("build_number")))

    def _fetch_revision_to_build_map(self):
        # All _fetch requests go through _buildbot for easier mocking
        # FIXME: This should use NetworkTransaction's 404 handling instead.
        try:
            # FIXME: This method is horribly slow due to the huge network load.
            # FIXME: This is a poor way to do revision -> build mapping.
            # Better would be to ask buildbot through some sort of API.
            print "Loading revision/build list from %s." % self.results_url()
            print "This may take a while..."
            result_files = self._buildbot._fetch_twisted_directory_listing(self.results_url())
        except urllib2.HTTPError, error:
            if error.code != 404:
                raise
            result_files = []

        # This assumes there was only one build per revision, which is false but we don't care for now.
        return dict([self._revision_and_build_for_filename(file_info["filename"]) for file_info in result_files])

    def _revision_to_build_map(self):
        if not self._revision_to_build_number:
            self._revision_to_build_number = self._fetch_revision_to_build_map()
        return self._revision_to_build_number

    def revision_build_pairs_with_results(self):
        return self._revision_to_build_map().items()

    # This assumes there can be only one build per revision, which is false, but we don't care for now.
    def build_for_revision(self, revision, allow_failed_lookups=False):
        # NOTE: This lookup will fail if that exact revision was never built.
        build_number = self._revision_to_build_map().get(int(revision))
        if not build_number:
            return None
        build = self.build(build_number)
        if not build and allow_failed_lookups:
            # Builds for old revisions with fail to lookup via buildbot's xmlrpc api.
            build = Build(self,
                build_number=build_number,
                revision=revision,
                is_green=False,
            )
        return build

    def find_regression_window(self, red_build, look_back_limit=30):
        if not red_build or red_build.is_green():
            return RegressionWindow(None, None)
        common_failures = None
        current_build = red_build
        build_after_current_build = None
        look_back_count = 0
        while current_build:
            if current_build.is_green():
                # current_build can't possibly have any failures in common
                # with red_build because it's green.
                break
            results = current_build.layout_test_results()
            # We treat a lack of results as if all the test failed.
            # This occurs, for example, when we can't compile at all.
            if results:
                failures = set(results.failing_tests())
                if common_failures == None:
                    common_failures = failures
                else:
                    common_failures = common_failures.intersection(failures)
                    if not common_failures:
                        # current_build doesn't have any failures in common with
                        # the red build we're worried about.  We assume that any
                        # failures in current_build were due to flakiness.
                        break
            look_back_count += 1
            if look_back_count > look_back_limit:
                return RegressionWindow(None, current_build, failing_tests=common_failures)
            build_after_current_build = current_build
            current_build = current_build.previous_build()
        # We must iterate at least once because red_build is red.
        assert(build_after_current_build)
        # Current build must either be green or have no failures in common
        # with red build, so we've found our failure transition.
        return RegressionWindow(current_build, build_after_current_build, failing_tests=common_failures)

    def find_blameworthy_regression_window(self, red_build_number, look_back_limit=30, avoid_flakey_tests=True):
        red_build = self.build(red_build_number)
        regression_window = self.find_regression_window(red_build, look_back_limit)
        if not regression_window.build_before_failure():
            return None  # We ran off the limit of our search
        # If avoid_flakey_tests, require at least 2 bad builds before we
        # suspect a real failure transition.
        if avoid_flakey_tests and regression_window.failing_build() == red_build:
            return None
        return regression_window


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

    def _fetch_results_html(self):
        results_html = "%s/results.html" % (self.results_url())
        # FIXME: This should use NetworkTransaction's 404 handling instead.
        try:
            # It seems this can return None if the url redirects and then returns 404.
            return urllib2.urlopen(results_html)
        except urllib2.HTTPError, error:
            if error.code != 404:
                raise

    def layout_test_results(self):
        if not self._layout_test_results:
            # FIXME: This should cache that the result was a 404 and stop hitting the network.
            self._layout_test_results = LayoutTestResults.results_from_string(self._fetch_results_html())
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

        # If any core builder is red we should not be landing patches.  Other
        # builders should be added to this list once they are known to be
        # reliable.
        # See https://bugs.webkit.org/show_bug.cgi?id=33296 and related bugs.
        self.core_builder_names_regexps = [
            "SnowLeopard.*Build",
            "SnowLeopard.*\(Test",  # Exclude WebKit2 for now.
            "Leopard",
            "Tiger",
            "Windows.*Build",
            "GTK.*32",
            "GTK.*64.*Debug",  # Disallow the 64-bit Release bot which is broken.
            "Qt",
            "Chromium.*Release$",
        ]

    def _parse_last_build_cell(self, builder, cell):
        status_link = cell.find('a')
        if status_link:
            # Will be either a revision number or a build number
            revision_string = status_link.string
            # If revision_string has non-digits assume it's not a revision number.
            builder['built_revision'] = int(revision_string) \
                                        if not re.match('\D', revision_string) \
                                        else None

            # FIXME: We treat slave lost as green even though it is not to
            # work around the Qts bot being on a broken internet connection.
            # The real fix is https://bugs.webkit.org/show_bug.cgi?id=37099
            builder['is_green'] = not re.search('fail', cell.renderContents()) or \
                                  not not re.search('lost', cell.renderContents())

            status_link_regexp = r"builders/(?P<builder_name>.*)/builds/(?P<build_number>\d+)"
            link_match = re.match(status_link_regexp, status_link['href'])
            builder['build_number'] = int(link_match.group("build_number"))
        else:
            # We failed to find a link in the first cell, just give up.  This
            # can happen if a builder is just-added, the first cell will just
            # be "no build"
            # Other parts of the code depend on is_green being present.
            builder['is_green'] = False
            builder['built_revision'] = None
            builder['build_number'] = None

    def _parse_current_build_cell(self, builder, cell):
        activity_lines = cell.renderContents().split("<br />")
        builder["activity"] = activity_lines[0] # normally "building" or "idle"
        # The middle lines document how long left for any current builds.
        match = re.match("(?P<pending_builds>\d) pending", activity_lines[-1])
        builder["pending_builds"] = int(match.group("pending_builds")) if match else 0

    def _parse_builder_status_from_row(self, status_row):
        status_cells = status_row.findAll('td')
        builder = {}

        # First cell is the name
        name_link = status_cells[0].find('a')
        builder["name"] = unicode(name_link.string)

        self._parse_last_build_cell(builder, status_cells[1])
        self._parse_current_build_cell(builder, status_cells[2])
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

    def idle_red_core_builders(self):
        return [builder for builder in self.red_core_builders() if builder["activity"] == "idle"]

    def core_builders_are_green(self):
        return not self.red_core_builders()

    # FIXME: These _fetch methods should move to a networking class.
    def _fetch_xmlrpc_build_dictionary(self, builder, build_number):
        # The buildbot XMLRPC API is super-limited.
        # For one, you cannot fetch info on builds which are incomplete.
        proxy = xmlrpclib.ServerProxy("http://%s/xmlrpc" % self.buildbot_host, allow_none=True)
        try:
            return proxy.getBuild(builder.name(), int(build_number))
        except xmlrpclib.Fault, err:
            build_url = Build.build_url(builder, build_number)
            _log.error("Error fetching data for %s build %s (%s): %s" % (builder.name(), build_number, build_url, err))
            return None

    def _fetch_one_box_per_builder(self):
        build_status_url = "http://%s/one_box_per_builder" % self.buildbot_host
        return urllib2.urlopen(build_status_url)

    def _file_cell_text(self, file_cell):
        """Traverses down through firstChild elements until one containing a string is found, then returns that string"""
        element = file_cell
        while element.string is None and element.contents:
            element = element.contents[0]
        return element.string

    def _parse_twisted_file_row(self, file_row):
        string_or_empty = lambda string: unicode(string) if string else u""
        file_cells = file_row.findAll('td')
        return {
            "filename": string_or_empty(self._file_cell_text(file_cells[0])),
            "size": string_or_empty(self._file_cell_text(file_cells[1])),
            "type": string_or_empty(self._file_cell_text(file_cells[2])),
            "encoding": string_or_empty(self._file_cell_text(file_cells[3])),
        }

    def _parse_twisted_directory_listing(self, page):
        soup = BeautifulSoup(page)
        # HACK: Match only table rows with a class to ignore twisted header/footer rows.
        file_rows = soup.find('table').findAll('tr', {'class': re.compile(r'\b(?:directory|file)\b')})
        return [self._parse_twisted_file_row(file_row) for file_row in file_rows]

    # FIXME: There should be a better way to get this information directly from twisted.
    def _fetch_twisted_directory_listing(self, url):
        return self._parse_twisted_directory_listing(urllib2.urlopen(url))

    def builders(self):
        return [self.builder_with_name(status["name"]) for status in self.builder_statuses()]

    # This method pulls from /one_box_per_builder as an efficient way to get information about
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

    def failure_map(self, only_core_builders=True):
        builder_statuses = self.core_builder_statuses() if only_core_builders else self.builder_statuses()
        failure_map = FailureMap()
        revision_to_failing_bots = {}
        for builder_status in builder_statuses:
            if builder_status["is_green"]:
                continue
            builder = self.builder_with_name(builder_status["name"])
            regression_window = builder.find_blameworthy_regression_window(builder_status["build_number"])
            if regression_window:
                failure_map.add_regression_window(builder, regression_window)
        return failure_map

    # This makes fewer requests than calling Builder.latest_build would.  It grabs all builder
    # statuses in one request using self.builder_statuses (fetching /one_box_per_builder instead of builder pages).
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
