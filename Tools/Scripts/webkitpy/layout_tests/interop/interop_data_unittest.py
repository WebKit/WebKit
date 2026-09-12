# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest
from datetime import date

from webkitpy.layout_tests.interop.interop_data import (
    DEFAULT_CATEGORY_URL,
    INTEROP_DATA_URL,
    METADATA_URL,
    InteropDataError,
    InteropYear,
    LabelledTestFinder,
    find_wpt_tests,
)

INTEROP_DATA = {
    '2026': {
        'focus_areas': {
            'scroll-snap': {
                'description': 'Scroll Snap',
                'countsTowardScore': True,
                'labels': ['interop-2026-scroll-snap'],
            },
            'webrtc': {
                'description': 'WebRTC',
                'countsTowardScore': False,
                'labels': ['interop-2026-webrtc'],
            },
        },
    },
}

CATEGORY_DATA = {
    '2026': {
        'categories': [{
            'name': 'Scroll Snap and friends',
            'labels': ['interop-2026-scroll-snap', 'interop-2026-scroll-animations'],
        }],
    },
}

METADATA = {
    '/css/css-scroll-snap/snap-area.html': [{'label': 'interop-2026-scroll-snap'}],
    '/css/css-scroll-snap/scroll-snap-type.html': [{'label': 'interop-2026-scroll-snap'}],
    '/css/css-scroll-snap/nested/*': [{'label': 'interop-2026-scroll-snap'}],
    '/scroll-animations/view-timeline.html': [{'label': 'interop-2026-scroll-animations'}],
    '/webrtc/RTCPeerConnection.html': [{'label': 'interop-2026-webrtc'}],
    '/dom/Node-appendChild.html': [{'url': 'https://webkit.org/b/1'}],
}

DURING_2026 = date(2026, 7, 29)


class MockJSONFetcher(object):
    def __init__(self):
        self.fetched_urls = []

    def __call__(self, url):
        self.fetched_urls.append(url)
        if url.endswith(INTEROP_DATA_URL):
            return INTEROP_DATA
        if url == DEFAULT_CATEGORY_URL:
            return CATEGORY_DATA
        if url.endswith(METADATA_URL):
            return METADATA
        raise AssertionError('unexpected URL: {}'.format(url))


class InteropYearTest(unittest.TestCase):
    def test_year_is_the_start_year(self):
        self.assertEqual(2026, InteropYear(start_date=date(2026, 2, 12), end_date=date(2027, 12, 31)).year)

    def test_end_before_start_is_rejected(self):
        with self.assertRaises(ValueError):
            InteropYear(start_date=date(2026, 2, 12), end_date=date(2025, 2, 12))


class LabelledTestFinderTest(unittest.TestCase):
    def finder(self):
        return LabelledTestFinder(json_fetcher=MockJSONFetcher())

    def test_tests_for_labels(self):
        tests_for_labels = self.finder().tests_for_labels()
        self.assertEqual({
            '/css/css-scroll-snap/snap-area.html',
            '/css/css-scroll-snap/scroll-snap-type.html',
            # A "/*" directory entry keeps its trailing slash.
            '/css/css-scroll-snap/nested/',
        }, tests_for_labels['interop-2026-scroll-snap'])
        self.assertEqual({'/webrtc/RTCPeerConnection.html'}, tests_for_labels['interop-2026-webrtc'])
        # Metadata without a label (e.g. a bug link) isn't a label.
        self.assertNotIn('/dom/Node-appendChild.html', set().union(*tests_for_labels.values()))

    def test_category_for_focus_area(self):
        self.assertEqual('scroll-snap', self.finder().category_for_focus_area(2026, 'Scroll Snap'))

    def test_category_for_unknown_focus_area(self):
        with self.assertRaises(InteropDataError):
            self.finder().category_for_focus_area(2026, 'Scroll Snapp')

    def test_category_for_focus_area_in_unknown_year(self):
        with self.assertRaises(InteropDataError):
            self.finder().category_for_focus_area(2019, 'Scroll Snap')

    def test_categories_for_year(self):
        self.assertEqual({'scroll-snap'}, self.finder().categories_for_year(2026))
        self.assertEqual({'scroll-snap', 'webrtc'}, self.finder().categories_for_year(2026, only_active=False))

    def test_interop_scoring_categories_require_no_only_active(self):
        with self.assertRaises(InteropDataError):
            self.finder().categories_for_year(2026, only_active=True, use_interop_scoring_categories=True)
        self.assertEqual(
            {'Scroll Snap and friends'},
            self.finder().categories_for_year(2026, only_active=False, use_interop_scoring_categories=True),
        )

    def test_labels_for_categories(self):
        self.assertEqual(
            {'scroll-snap': {'interop-2026-scroll-snap'}, 'webrtc': {'interop-2026-webrtc'}},
            self.finder().labels_for_categories(2026),
        )
        self.assertEqual(
            {'Scroll Snap and friends': {'interop-2026-scroll-snap', 'interop-2026-scroll-animations'}},
            self.finder().labels_for_categories(2026, use_interop_scoring_labels=True),
        )

    def test_discover_years_from_data(self):
        self.assertEqual({2026}, self.finder().discover_years_from_data())
        self.assertEqual({2026}, self.finder().discover_years_from_data(use_interop_scoring_categories=True))

    def test_data_is_only_fetched_once(self):
        fetcher = MockJSONFetcher()
        finder = LabelledTestFinder(json_fetcher=fetcher)
        finder.tests_for_labels()
        finder.tests_for_labels()
        self.assertEqual(1, len(fetcher.fetched_urls))


class FindWPTTestsTest(unittest.TestCase):
    def find(self, fetcher=None, **kwargs):
        fetcher = fetcher if fetcher is not None else MockJSONFetcher()
        kwargs.setdefault('today', DURING_2026)
        return find_wpt_tests(finder=LabelledTestFinder(json_fetcher=fetcher), **kwargs)

    def test_labels(self):
        self.assertEqual([
            '/css/css-scroll-snap/nested/',
            '/css/css-scroll-snap/scroll-snap-type.html',
            '/css/css-scroll-snap/snap-area.html',
        ], self.find(labels=['interop-2026-scroll-snap']))

    def test_labels_alone_only_fetch_metadata(self):
        fetcher = MockJSONFetcher()
        self.find(fetcher=fetcher, labels=['interop-2026-scroll-snap'])
        self.assertEqual(1, len(fetcher.fetched_urls))
        self.assertTrue(fetcher.fetched_urls[0].endswith(METADATA_URL), fetcher.fetched_urls)

    def test_unknown_label_finds_nothing(self):
        self.assertEqual([], self.find(labels=['interop-2026-scroll-snapp']))

    def test_focus_areas(self):
        self.assertEqual([
            '/css/css-scroll-snap/nested/',
            '/css/css-scroll-snap/scroll-snap-type.html',
            '/css/css-scroll-snap/snap-area.html',
        ], self.find(years=[2026], focus_areas=['Scroll Snap']))

    def test_categories(self):
        self.assertEqual(
            ['/webrtc/RTCPeerConnection.html'],
            self.find(years=[2026], categories=['webrtc']),
        )

    def test_interop_scoring_categories(self):
        self.assertEqual([
            '/css/css-scroll-snap/nested/',
            '/css/css-scroll-snap/scroll-snap-type.html',
            '/css/css-scroll-snap/snap-area.html',
            '/scroll-animations/view-timeline.html',
        ], self.find(
            years=[2026],
            categories=['Scroll Snap and friends'],
            use_interop_scoring_labels=True,
        ))

    def test_no_selection_defaults_to_active_categories_of_active_years(self):
        # WebRTC doesn't count toward the score, so it isn't active.
        self.assertEqual([
            '/css/css-scroll-snap/nested/',
            '/css/css-scroll-snap/scroll-snap-type.html',
            '/css/css-scroll-snap/snap-area.html',
        ], self.find())

    def test_no_selection_with_inactive_categories(self):
        self.assertEqual([
            '/css/css-scroll-snap/nested/',
            '/css/css-scroll-snap/scroll-snap-type.html',
            '/css/css-scroll-snap/snap-area.html',
            '/webrtc/RTCPeerConnection.html',
        ], self.find(only_active=False))

    def test_no_labels_to_search_for(self):
        # No Interop year is active in 2019, so there are no categories to expand.
        with self.assertRaises(InteropDataError):
            self.find(today=date(2019, 7, 29))


if __name__ == '__main__':
    unittest.main()
