# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""The pages, and the promise that serving one cannot reach the network."""

from __future__ import annotations

import datetime
import functools
import re
import time
from typing import Optional
from unittest import mock
from urllib.parse import parse_qs, quote, urlsplit

from ews_dashboard import config, results, suites
from ews_dashboard.analysis import convictions, escapes, false_positive, filters, freshness, trend
from ews_dashboard.web import app as web_app, formatting
from tests import fixtures

RELIABLE = 99.5
UNRELIABLE = 40.0

# The query arguments the tests page's convicted-tests table is filtered and ordered by.
FILTER = filters.filter_argument(filters.TESTS)
SORT = filters.sort_argument(filters.TESTS)


class WebTest(fixtures.DatabaseTest):
    def setUp(self) -> None:
        super().setUp()
        self.client = web_app.create_app(self.database_path).test_client()

    def store_build(self, *arguments: object, **keywords: object) -> int:
        keywords.setdefault('started_at', int(time.time()) - 3600)
        return super().store_build(*arguments, **keywords)

    def classify_everything(self, pass_rates: dict, days: int = 1) -> None:
        now = int(time.time())
        false_positive.rate(
            self.connection,
            false_positive.live_classifier(self.connection, fixtures.StubHistory(pass_rates)),
            now - days * 86400,
            now,
        )

    def record_refresh(self, finished_at: int) -> None:
        with self.connection:
            self.connection.execute(
                'INSERT INTO refresh_runs (started_at, finished_at) VALUES (?, ?)',
                (finished_at - 60, finished_at),
            )

    def page(self, path: str = '/') -> str:
        response = self.client.get(path)
        self.assertEqual(response.status_code, 200, path)
        return response.get_data(as_text=True)

    def builds_pane(self, page: str) -> str:
        """Just the failing-builds pane. Every link on the page now carries the build open beside it,
        so a regex for an entry linking to a build matches the queue rail as readily."""
        start = page.index('<div class="pane builds">')
        return page[start:page.index('<div class="pane detail">', start)]

    def entry_classes(self, page: str, build_id: int) -> str:
        """The classes on one build's entry in the builds pane, found by the build it links to."""
        entry = re.search(rf'<a class="(entry[^"]*)" href="[^"]*build={build_id}[&"]',
                          self.builds_pane(page))
        self.assertIsNotNone(entry, f'no builds-pane entry linking to build {build_id}')
        return entry.group(1)

    def convicted_section(self, page: str) -> str:
        """The convicted-tests section alone, cut off before the next section (there is none today,
        but a helper that trusted there never would be would break silently the day one is added)."""
        start = page.index('<div class="section col-7" id="convicted">')
        end = page.find('<div class="section" id="', start + 1)
        return page[start:] if end == -1 else page[start:end]

    def detail_pane(self, page: str) -> str:
        """The test drilldown alone, cut off before the vocabulary legend that follows it on every
        `/tests` page — the legend has its own `<table>` and `<tr>` markup that a caller counting or
        excluding tags in the drilldown must not also be counting."""
        start = page.index('id="test-detail"')
        end = page.index('<details class="section legend-pane"', start)
        return page[start:end]

    def sort_link(self, page: str, key: str) -> str:
        """The href of the column heading that would order a table by one key."""
        found = re.search(rf'<th class="sortable[^"]*"><a href="([^"]*{re.escape(SORT)}={key}:'
                          r'[^"]*)"', page)
        self.assertIsNotNone(found, f'no heading offered to sort by {key}')
        return found.group(1).replace('&amp;', '&')

    def chooser_link(self, page: str, label: str) -> str:
        """The href of one scope chooser's badge, found by the label it shows."""
        found = re.search(rf'<a class="badge[^"]*" href="([^"]*)">{re.escape(label)}</a>', page)
        self.assertIsNotNone(found, f'no chooser badge labelled {label}')
        return found.group(1).replace('&amp;', '&')


class TestWindow(WebTest):
    """Anchored to the end of the UTC day, a window held less than it claimed: half an hour past
    midnight the shortest one held half an hour of EWS, and a build from yesterday evening was
    outside a window said to cover a week."""

    JUST_AFTER_MIDNIGHT = int(datetime.datetime(2026, 8, 27, 0, 30,
                                                tzinfo=datetime.timezone.utc).timestamp())

    def page_at_midnight(self, path: str) -> str:
        with mock.patch('ews_dashboard.web.app.time.time',
                        return_value=self.JUST_AFTER_MIDNIGHT):
            return self.page(path)

    def test_a_window_reaches_a_full_span_back_from_now(self) -> None:
        with mock.patch('ews_dashboard.web.app.time.time',
                        return_value=self.JUST_AFTER_MIDNIGHT):
            window = web_app.Window.of_days(7)
        self.assertEqual((window.since, window.until),
                         (self.JUST_AFTER_MIDNIGHT - 7 * 86400, self.JUST_AFTER_MIDNIGHT))

    def test_a_build_from_the_far_edge_of_the_window_is_still_inside_it(self) -> None:
        """Six days and an hour old: inside a rolling week, outside a week that ended at midnight."""
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                         started_at=self.JUST_AFTER_MIDNIGHT - 6 * 86400 - 3600)
        self.assertNotIn('No failing builds in this window.',
                         self.page_at_midnight('/explore?days=7'))

    def test_a_build_older_than_the_window_is_left_out(self) -> None:
        """`state=all` keeps this about the window, not about the default two-noise-state filter."""
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                         started_at=self.JUST_AFTER_MIDNIGHT - 7 * 86400 - 3600)
        self.assertIn('No failing builds in this window.',
                      self.page_at_midnight(f'/explore?days=7&state={formatting.ANY_STATE}'))


class TestLanding(WebTest):
    def test_an_empty_database_renders_and_says_so_rather_than_claiming_a_perfect_score(self) -> None:
        page = self.page('/')
        self.assertIn('No classified builds', page)
        self.assertIn(f'<span class="value">{formatting.MISSING}</span>', page)
        self.assertIn('0 of 0 failing builds', page)

    def test_a_never_refreshed_database_is_stale(self) -> None:
        self.assertIn('freshness stale', self.page('/'))

    def test_a_recent_refresh_is_not_stale(self) -> None:
        self.record_refresh(int(time.time()) - 300)
        self.assertNotIn('freshness stale', self.page('/'))

    def test_the_rate_appears_with_the_counts_behind_it(self) -> None:
        self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[])
        self.store_build(2, first=['fast/real.html'], second=['fast/real.html'], clean=[])
        self.classify_everything({'fast/pre.html': UNRELIABLE, 'fast/real.html': RELIABLE})
        page = self.page('/')
        self.assertIn('<span class="value">50%</span>', page)
        self.assertIn('1 of 2 failing builds', page)

    def test_an_unclassified_build_is_disclosed_rather_than_scored(self) -> None:
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        page = self.page('/')
        self.assertIn('1 failing builds in this window have not been classified yet', page)

    def test_convictions_are_listed_per_rule_including_the_rules_that_never_fired(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        page = self.page('/')
        for rule in config.FLAKINESS_RULES:
            self.assertIn(rule, page)

    def test_the_queue_chooser_shows_all_active_when_no_group_is_chosen(self) -> None:
        """The suite chooser's own `all` badge defaults active, and the queue picker's own summary
        defaults unmarked, so nothing reads as narrowed when neither has been."""
        page = self.page('/')
        self.assertEqual(len(re.findall(r'<a class="badge active" href="[^"]*">all</a>', page)), 1)
        self.assertRegex(page, r'<summary class="" aria-label="Filter by queue">')
        self.assertNotRegex(page, r'<input type="checkbox" name="group"[^>]* checked')

    def test_the_queue_chooser_marks_a_chosen_group_active_and_leaves_others_off(self) -> None:
        self.store_build(1, flaky={'fast/gtk.html': config.CLEAN_TREE},
                         builder=fixtures.GTK_BUILDER, builder_id=9)
        self.store_build(2, flaky={'fast/wpe.html': config.CLEAN_TREE},
                         builder=fixtures.WPE_BUILDER, builder_id=10)
        page = self.page('/?group=GTK')
        self.assertRegex(page, r'<summary class="active" aria-label="Filter by queue">')
        self.assertRegex(page, r'<input type="checkbox" name="group" value="GTK"[^>]* checked>')
        self.assertRegex(page, r'<input type="checkbox" name="group" value="WPE" aria-label="Select WPE">')

    def test_the_queue_picker_offers_every_queue_even_when_a_group_narrows_the_counts(self) -> None:
        """Choosing a group narrows what the page counts, but the picker still offers every queue in
        the window -- narrowing which queues are *offered* is not what a group filter does."""
        self.store_build(1, flaky={'fast/mac.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/gtk.html': config.CLEAN_TREE},
                         builder=fixtures.GTK_BUILDER, builder_id=9)
        page = self.page('/?group=GTK')
        self.assertRegex(page,
                         rf'<input type="checkbox" name="builder" value="{re.escape(fixtures.GTK_BUILDER)}"')
        self.assertNotRegex(page,
                            rf'name="builder" value="{re.escape(fixtures.LAYOUT_BUILDER)}"[^>]* checked')
        self.assertIn('<span class="value">1</span>', page)


class TestFreshnessDismissal(WebTest):
    """Dismissing the freshness banner is a request rather than a click handler, since the app ships
    no JavaScript and a banner hidden on one page has to stay hidden on the next."""

    BANNER = 'class="freshness'

    def dismiss_link(self, path: str = '/') -> str:
        found = re.search(r'<a class="dismiss" href="([^"]+)"', self.page(path))
        self.assertIsNotNone(found, f'no dismissal control on {path}')
        return found.group(1).replace('&amp;', '&')

    def test_the_control_names_what_it_dismisses_and_where_it_returns(self) -> None:
        link = urlsplit(self.dismiss_link('/explore?days=14'))
        self.assertEqual(link.path, '/dismiss-freshness')
        arguments = parse_qs(link.query)
        self.assertEqual(arguments['state'], [freshness.current(self.connection).signature])
        self.assertEqual(arguments['next'], ['/explore?days=14'])

    def test_dismissing_returns_to_the_page_it_was_dismissed_from(self) -> None:
        response = self.client.get(self.dismiss_link('/explore?days=14'))
        self.assertEqual(response.status_code, 302)
        self.assertTrue(response.headers['Location'].endswith('/explore?days=14'),
                        response.headers['Location'])

    def test_a_dismissal_holds_on_every_page_and_not_only_the_one_it_was_made_from(self) -> None:
        self.client.get(self.dismiss_link('/'))
        self.assertNotIn(self.BANNER, self.page('/'))
        self.assertNotIn(self.BANNER, self.page('/explore'))

    def test_a_dismissal_does_not_outlive_the_banner_it_dismissed(self) -> None:
        """Hiding "nothing has refreshed this database yet" must not go on to hide a refresh that
        died, so the banner comes back once it has something else to say."""
        self.client.get(self.dismiss_link('/'))
        self.record_refresh(int(time.time()) - 300)
        self.assertIn(self.BANNER, self.page('/'))

    def test_a_dismissal_cannot_send_a_reader_off_the_site(self) -> None:
        for target in ('https://example.com/', '//example.com/', '/\\example.com'):
            response = self.client.get(f'/dismiss-freshness?state=x&next={quote(target)}')
            self.assertEqual(response.headers['Location'], '/', target)

    def test_the_trend_is_drawn_as_inline_svg_with_hover_titles(self) -> None:
        self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[])
        self.classify_everything({'fast/pre.html': UNRELIABLE})
        page = self.page('/')
        self.assertIn('<svg', page)
        self.assertIn('builds blamed an author for noise', page)
        # The chart itself is inline svg, needing no script of its own; the one script this page does
        # carry is the deferred, page-wide progressive enhancement file, not an inline handler.
        self.assertEqual(page.count('<script'), 1)
        self.assertIn('<script defer src="/static/dashboard.js"></script>', page)

    def test_a_window_choice_is_linkable_and_an_unknown_one_falls_back(self) -> None:
        self.assertIn('last 30 days', self.page('/?days=30').lower())
        self.assertIn('last 7 days', self.page('/?days=nonsense').lower())
        self.assertIn('last 7 days', self.page('/?days=9999').lower())

    def test_the_chart_spans_the_window_the_rest_of_the_page_counts_over(self) -> None:
        self.assertIn('Blame noise over 14 days', self.page('/?days=14'))
        self.assertIn('Blame noise over 7 days', self.page('/'))

    def rolling_segments(self, page: str) -> int:
        """How many segments the rolling line the page drew is made of, one per pair of covered days."""
        drawn = re.findall(r'<line class="rolling"', page)
        self.assertTrue(drawn, 'the page drew no rolling line')
        return len(drawn)

    def test_a_rolling_average_is_linkable_and_an_unknown_one_falls_back(self) -> None:
        self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[])
        self.classify_everything({'fast/pre.html': UNRELIABLE})
        self.assertIn('14-day rolling', self.page('/?rolling=14'))
        self.assertIn(f'{trend.ROLLING_DAYS}-day rolling', self.page('/?rolling=5'))
        self.assertIn(f'{trend.ROLLING_DAYS}-day rolling', self.page('/?rolling=nonsense'))

    def test_a_longer_rolling_average_still_covers_days_a_shorter_one_has_dropped(self) -> None:
        self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[],
                         started_at=int(time.time()) - 5 * 86400)
        self.classify_everything({'fast/pre.html': UNRELIABLE}, days=6)
        self.assertGreater(self.rolling_segments(self.page('/?rolling=14')),
                           self.rolling_segments(self.page('/?rolling=3')))

    def _blamed_here_clean_elsewhere(self) -> None:
        """One blamed build on the layout queue, one clean build on another, so a filter that does
        not narrow and a filter that narrows to the wrong queue both read differently."""
        self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[])
        self.store_build(2, first=['fast/real.html'], second=['fast/real.html'], clean=[],
                         builder=fixtures.GTK_BUILDER, builder_id=11)
        self.classify_everything({'fast/pre.html': UNRELIABLE, 'fast/real.html': RELIABLE})

    def test_a_queue_filter_narrows_the_cards_and_the_chart(self) -> None:
        self._blamed_here_clean_elsewhere()
        whole = self.page('/')
        self.assertIn('<span class="value">50%</span>', whole)
        self.assertIn('1 of 2 failing builds', whole)
        self.assertIn('1 of 2 builds blamed an author for noise', whole)

        narrowed = self.page(f'/?builder={fixtures.GTK_BUILDER}')
        self.assertIn('<span class="value">0%</span>', narrowed)
        self.assertIn('0 of 1 failing builds', narrowed)
        self.assertIn('0 of 1 builds blamed an author for noise', narrowed)

    def test_a_suite_filter_narrows_the_cards_and_the_chart(self) -> None:
        self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[])
        self.store_build(2, first=['api/pre.html'], second=['api/pre.html'], clean=[],
                         builder=fixtures.API_BUILDER, builder_id=9)
        self.classify_everything({'fast/pre.html': UNRELIABLE, 'api/pre.html': UNRELIABLE})
        api = suites.suite_for_builder(fixtures.API_BUILDER).name
        narrowed = self.page(f'/?suite={api}')
        self.assertIn('1 of 1 failing builds', narrowed)
        self.assertIn('1 of 1 builds blamed an author for noise', narrowed)

    def test_an_unknown_queue_is_ignored_rather_than_emptying_the_page(self) -> None:
        self._blamed_here_clean_elsewhere()
        self.assertIn('1 of 2 failing builds', self.page('/?builder=not-a-queue'))

    def queue_tree_form(self, page: str) -> str:
        """The queue picker's own form: the hidden fields it holds the rest of the scope in, and the
        checkbox tree a reader picks a queue from."""
        found = re.search(r'<details class="table-filter queue-tree".*?</form>\s*</details>', page, re.S)
        self.assertIsNotNone(found, 'no queue picker form on the page')
        return found.group(0)

    def test_the_queue_picker_form_keeps_the_rest_of_the_scope_in_its_hidden_fields(self) -> None:
        self._blamed_here_clean_elsewhere()
        form = self.queue_tree_form(self.page('/?days=14&rolling=3'))
        self.assertIn('<input type="hidden" name="days" value="14">', form)
        self.assertIn('<input type="hidden" name="rolling" value="3">', form)

    def test_the_chosen_queue_is_marked_checked_in_the_tree_and_another_is_not(self) -> None:
        self._blamed_here_clean_elsewhere()
        form = self.queue_tree_form(self.page(f'/?builder={fixtures.GTK_BUILDER}'))
        self.assertRegex(form, rf'name="builder" value="{re.escape(fixtures.GTK_BUILDER)}" checked')
        self.assertNotRegex(form, rf'name="builder" value="{re.escape(fixtures.LAYOUT_BUILDER)}" checked')

    def test_the_queue_picker_form_holds_no_hidden_copy_of_the_queue_selection(self) -> None:
        """The picker's own checkboxes are the queue selection, so its form cannot also carry a
        hidden `group`/`version`/`builder`: a hidden copy is what let an unticked checkbox keep
        submitting the old value, since the browser sends nothing for an unticked box but a hidden
        input beside it still would."""
        self._blamed_here_clean_elsewhere()
        form = self.queue_tree_form(self.page('/?group=GTK'))
        self.assertNotRegex(form, r'<input type="hidden" name="group"')
        self.assertNotRegex(form, r'<input type="hidden" name="version"')
        self.assertNotRegex(form, r'<input type="hidden" name="builder"')


class TestTests(WebTest):
    def test_a_convicted_test_opens_its_drilldown_and_links_to_all_configurations_and_its_build(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        page = self.page('/tests')
        self.assertIn('<a href="/tests?test=fast/a.html">fast/a.html</a>', page)
        self.assertIn('results.webkit.org/?suite=layout-tests&amp;test=fast%2Fa.html">all configurations',
                      page)
        self.assertIn('ews-build.webkit.org/#/builders/7/builds/1', page)
        self.assertIn('github.com/WebKit/WebKit/pull/12345', page)

    def test_nothing_convicted_says_so(self) -> None:
        self.assertIn('Nothing convicted in this window', self.page('/tests'))

    def test_a_builder_filter_narrows_the_page(self) -> None:
        self.store_build(1, flaky={'fast/layout.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/api.html': config.CLEAN_TREE},
                         builder=fixtures.API_BUILDER, builder_id=9)
        page = self.page(f'/tests?builder={fixtures.API_BUILDER}')
        self.assertIn('fast/api.html', page)
        self.assertNotIn('fast/layout.html', page)

    def test_an_unknown_builder_is_ignored_rather_than_filtering_everything_out(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.assertIn('fast/a.html', self.page('/tests?builder=not-a-queue'))

    def test_the_queue_picker_form_holds_no_hidden_copy_of_the_queue_selection(self) -> None:
        """The picker's own checkboxes are the queue selection, so its form cannot also carry a
        hidden `group`/`version`/`builder`: a hidden copy is what let an unticked checkbox keep
        submitting the old value, since the browser sends nothing for an unticked box but a hidden
        input beside it still would."""
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, builder=fixtures.GTK_BUILDER,
                         builder_id=9)
        page = self.page('/tests?group=GTK')
        tree_form = re.search(r'<details class="table-filter queue-tree".*?</form>\s*</details>', page, re.S)
        self.assertIsNotNone(tree_form, 'no queue picker form on the page')
        form = tree_form.group(0)
        self.assertNotRegex(form, r'<input type="hidden" name="group"')
        self.assertNotRegex(form, r'<input type="hidden" name="version"')
        self.assertNotRegex(form, r'<input type="hidden" name="builder"')

    def test_the_flake_type_column_shows_the_rule_and_its_description(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        page = self.page('/tests')
        self.assertIn(f'<span title="{config.RULE_DESCRIPTIONS[config.CLEAN_TREE]}">'
                      f'{config.CLEAN_TREE}</span>', page)

    def test_a_test_convicted_under_two_rules_is_one_row_whose_flake_types_are_both_listed(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/a.html': config.DIRTY_TREE})
        page = self.page('/tests')
        self.assertEqual(page.count('>fast/a.html<'), 1)
        self.assertIn(config.CLEAN_TREE, page)
        self.assertIn(config.DIRTY_TREE, page)

    def _convict_many(self, count: int) -> None:
        """One build per convicted test, since a table row is one per test."""
        for number in range(1, count + 1):
            self.store_build(number, flaky={f'fast/flake{number:03d}.html': config.CLEAN_TREE})

    def test_the_table_lists_every_convicted_test_within_the_cap(self) -> None:
        self._convict_many(5)
        page = self.page('/tests')
        self.assertEqual(page.count('all configurations'), 5)
        self.assertIn('All 5 convicted tests.', page)

    def test_a_table_that_fits_on_one_page_says_so_and_offers_no_way_deeper(self) -> None:
        self._convict_many(3)
        page = self.page('/tests')
        self.assertIn('All 3 convicted tests.', page)

    def test_a_flake_type_filter_narrows_the_table_to_that_rule(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/b.html': config.DIRTY_TREE})
        page = self.page(f'/tests?{FILTER}=rule:anyof:{config.DIRTY_TREE}')
        self.assertIn('fast/b.html', page)
        self.assertNotIn('fast/a.html', page)


class TestTestsSorting(WebTest):
    """A column heading is a link: this app needs no JavaScript, so an order is a URL, and it has to
    survive a hand-edited one."""

    SORTED_HEADER = re.compile(r'<th class="[^"]*\bsorted\b[^"]*"><a[^>]*>([^<]+)')

    def _convict_many(self, count: int) -> None:
        for number in range(1, count + 1):
            self.store_build(number, flaky={f'fast/flake{number:03d}.html': config.CLEAN_TREE})

    def _sorted_labels(self, path: str) -> set:
        return set(self.SORTED_HEADER.findall(self.page(path)))

    def test_the_column_asked_for_is_the_one_marked_sorted(self) -> None:
        self._convict_many(2)
        self.assertEqual(self._sorted_labels(f'/tests?{SORT}=test:asc'), {'Test'})
        self.assertEqual(self._sorted_labels(f'/tests?{SORT}=last_seen'), {'Last seen'})

    def test_nothing_asked_for_is_sorted_by_convictions(self) -> None:
        self._convict_many(2)
        self.assertEqual(self._sorted_labels('/tests'), {'Convictions'})

    def test_the_arrow_points_the_way_the_column_is_ordered_and_marks_no_other(self) -> None:
        """One arrow only: a window with nothing convicted renders a placeholder, not a table."""
        self._convict_many(2)
        ascending = self.page(f'/tests?{SORT}=test:asc')
        self.assertEqual(ascending.count('▴'), 1)
        self.assertNotIn('▾', ascending)
        self.assertIn('▾', self.page(f'/tests?{SORT}=test:desc'))

    def test_a_sort_argument_reorders_the_listed_tests(self) -> None:
        self.store_build(1, flaky={'fast/zzz.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/aaa.html': config.CLEAN_TREE})
        ascending = self.page(f'/tests?{SORT}=test:asc')
        self.assertLess(ascending.index('fast/aaa.html'), ascending.index('fast/zzz.html'))
        descending = self.page(f'/tests?{SORT}=test:desc')
        self.assertLess(descending.index('fast/zzz.html'), descending.index('fast/aaa.html'))

    def test_a_sort_key_that_is_not_a_key_still_renders_sorted_by_convictions(self) -> None:
        self._convict_many(2)
        self.assertEqual(self._sorted_labels(f'/tests?{SORT}=bogus'), {'Convictions'})

    def test_a_direction_that_is_not_a_direction_drops_the_key_it_was_asked_with(self) -> None:
        self._convict_many(2)
        self.assertEqual(self._sorted_labels(f'/tests?{SORT}=test:sideways'), {'Convictions'})

    def test_two_sorts_are_a_primary_and_a_secondary_key_in_the_order_they_were_written(self) -> None:
        """One test convicted twice and two convicted once: ordering by convictions alone leaves the
        pair in the tiebreak's order, so a secondary key is the only thing that can reverse them."""
        self.store_build(1, flaky={'fast/aaa.html': config.CLEAN_TREE,
                                   'fast/zzz.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/mmm.html': config.CLEAN_TREE})
        self.store_build(3, flaky={'fast/mmm.html': config.CLEAN_TREE})
        page = self.page(f'/tests?{SORT}=convictions:desc&{SORT}=test:desc')
        self.assertLess(page.index('fast/mmm.html'), page.index('fast/zzz.html'))
        self.assertLess(page.index('fast/zzz.html'), page.index('fast/aaa.html'))
        reversed_secondary = self.page(f'/tests?{SORT}=convictions:desc&{SORT}=test:asc')
        self.assertLess(reversed_secondary.index('fast/aaa.html'),
                        reversed_secondary.index('fast/zzz.html'))

    def test_the_secondary_key_is_the_second_one_and_not_the_first(self) -> None:
        """Asked for the other way round: the first key named leads and the second only breaks its
        ties. `test` cannot stand in for that first key the way it did in the sibling test above —
        the convicted-tests query groups by test name, so two different tests can never tie on it,
        and with no tie on the primary key this assertion would pass even if `order_by` dropped
        every key after the first. `queues` can tie two different tests, so it takes `test`'s place
        as the primary key here instead, with `convictions` breaking the tie it leaves."""
        self.store_build(1, flaky={'fast/aaa.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/zzz.html': config.CLEAN_TREE})
        self.store_build(3, flaky={'fast/zzz.html': config.CLEAN_TREE})
        self.store_build(4, flaky={'fast/mmm.html': config.CLEAN_TREE})
        self.store_build(5, flaky={'fast/mmm.html': config.CLEAN_TREE},
                         builder=fixtures.GTK_BUILDER, builder_id=11)
        page = self.page(f'/tests?{SORT}=queues:asc&{SORT}=convictions:desc')
        self.assertLess(page.index('fast/zzz.html'), page.index('fast/aaa.html'))
        self.assertLess(page.index('fast/aaa.html'), page.index('fast/mmm.html'))
        reversed_secondary = self.page(f'/tests?{SORT}=queues:asc&{SORT}=convictions:asc')
        self.assertLess(reversed_secondary.index('fast/aaa.html'),
                        reversed_secondary.index('fast/zzz.html'))

    def test_a_heading_link_returns_the_reader_to_the_table_it_belongs_to(self) -> None:
        self._convict_many(1)
        self.assertIn(f'{SORT}=test:asc#convicted', self.page('/tests'))

    def test_a_heading_link_keeps_the_flake_type_filter_and_the_table_filter(self) -> None:
        self._convict_many(2)
        link = self.sort_link(
            self.page(f'/tests?{FILTER}=rule:anyof:{config.CLEAN_TREE}&{FILTER}=test:has:flake'),
            'test')
        self.assertIn(f'{FILTER}=rule:anyof:{config.CLEAN_TREE}', link)
        self.assertIn(f'{FILTER}=test:has:flake', link)

    def test_a_heading_link_replaces_the_order_rather_than_carrying_the_old_one_too(self) -> None:
        """A heading names one key, so a click cannot leave the key it replaced in the URL to go on
        ordering the table ahead of it."""
        self._convict_many(2)
        link = self.sort_link(self.page(f'/tests?{SORT}=last_seen:desc'), 'test')
        self.assertIn(f'{SORT}=test:asc', link)
        self.assertNotIn('last_seen', link)

    def test_a_scope_change_keeps_the_flake_type_filter_and_the_order_it_is_being_read_in(self) -> None:
        self._convict_many(2)
        page = self.page(f'/tests?{FILTER}=rule:anyof:{config.CLEAN_TREE}&{SORT}=test:asc')
        for link in (self.chooser_link(page, '14d'), self.chooser_link(page, 'all')):
            self.assertIn(f'{FILTER}=rule:anyof:{config.CLEAN_TREE}', link)
            self.assertIn(f'{SORT}=test:asc', link)


class TestTestsTableCap(WebTest):
    """No pager: the table renders every convicted test up to a hard cap, and says so when the cap
    cuts the set."""

    def _convict_many(self, count: int, rule: str = config.CLEAN_TREE, first: int = 1) -> None:
        for number in range(first, first + count):
            self.store_build(number, flaky={f'fast/flake{number:03d}.html': rule})

    def _capped(self, limit: int) -> mock._patch:
        """Shrinks the row cap for one request, so a truncation test does not need thousands of
        builds to trigger it."""
        return mock.patch('ews_dashboard.web.app.convictions.convicted_tests',
                          functools.partial(convictions.convicted_tests, limit=limit))

    def test_the_cap_bounds_the_rows_rendered(self) -> None:
        self._convict_many(5)
        with self._capped(3):
            page = self.page('/tests')
        self.assertEqual(page.count('all configurations'), 3)

    def test_the_pane_names_how_many_of_how_many_when_the_cap_cuts_the_set(self) -> None:
        self._convict_many(5)
        with self._capped(3):
            page = self.page('/tests')
        self.assertIn('Showing the first 3 of 5 convicted tests.', self.convicted_section(page))

    def test_a_set_within_the_cap_reports_the_whole_count_with_no_truncation_notice(self) -> None:
        self._convict_many(3)
        page = self.page('/tests')
        section = self.convicted_section(page)
        self.assertIn('All 3 convicted tests.', section)
        self.assertNotIn('Showing the first', section)

    def test_a_stale_page_argument_is_ignored_rather_than_erroring(self) -> None:
        self._convict_many(3)
        page = self.page('/tests?page=2')
        section = self.convicted_section(page)
        self.assertIn('All 3 convicted tests.', section)
        self.assertEqual(page.count('all configurations'), 3)

    def test_the_table_carries_its_own_column_width_class(self) -> None:
        self._convict_many(1)
        section = self.convicted_section(self.page('/tests'))
        self.assertIn('<table class="table full-width convicted-tests">', section)


class TestTestsTableFilters(WebTest):
    """`f.tests=<column>:<condition>:<value>` is the whole of what a request can narrow a
    convicted-tests table by, and the only search this app has is the browser's: find-in-page cannot
    reach a row the server did not send, so narrowing is a request.

    The clause is repeatable and read in document order, and one that does not parse is dropped and
    said out loud rather than refusing the request, because a copied URL is the only state this page
    has and a 400 would take all of it away for one bad clause.
    """

    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={'editing/pasteboard/copy.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/forms/input.html': config.CLEAN_TREE})

    def chip_field(self, section: str, argument: str, index: int, field: str) -> str:
        """What one chip's `field` control currently holds: a select's selected option, or an
        input's value attribute — whichever the column at that index rendered as."""
        name = f'{argument}:{index}:{field}'
        select = re.search(rf'<select name="{re.escape(name)}"[^>]*>(.*?)</select>', section,
                           re.S)
        if select:
            selected = re.search(r'<option value="([^"]*)" selected', select.group(1))
            return selected.group(1) if selected else ''
        typed = re.search(rf'<input[^>]*name="{re.escape(name)}"[^>]*value="([^"]*)"', section)
        self.assertIsNotNone(typed, f'no chip control named {name}')
        return typed.group(1)

    def chip_count(self, section: str, kind: str) -> int:
        """How many chips a row of the given kind (`filter` or `sort`) currently shows."""
        row = re.search(rf'data-chip-kind="{kind}"[^>]*>(.*?)<button type="submit" name="add_{kind}"',
                        section, re.S)
        self.assertIsNotNone(row, f'no {kind} chip row')
        return row.group(1).count('<span class="chip">')

    def test_a_filter_lists_only_the_tests_that_match_and_counts_only_them(self) -> None:
        page = self.page(f'/tests?{FILTER}=test:has:editing')
        self.assertIn('editing/pasteboard/copy.html', page)
        self.assertNotIn('fast/forms/input.html', page)
        self.assertIn('All 1 tests matching test:has:editing.', page)

    def test_a_filter_reaches_a_column_that_is_not_the_name(self) -> None:
        """The registry offers seven columns and the old grammar could only ever ask about one."""
        self.store_build(3, flaky={'editing/pasteboard/copy.html': config.CLEAN_TREE})
        page = self.page(f'/tests?{FILTER}=convictions:ge:2')
        self.assertIn('editing/pasteboard/copy.html', page)
        self.assertNotIn('fast/forms/input.html', page)

    def test_two_filters_on_one_column_both_apply(self) -> None:
        self.store_build(3, flaky={'fast/dom/node.html': config.CLEAN_TREE})
        page = self.page(f'/tests?{FILTER}=test:has:fast&{FILTER}=test:nohas:forms')
        self.assertIn('fast/dom/node.html', page)
        self.assertNotIn('fast/forms/input.html', page)
        self.assertNotIn('editing/pasteboard/copy.html', page)

    def test_the_surface_offers_every_committed_filter_back_as_a_chip_in_order(self) -> None:
        section = self.convicted_section(
            self.page(f'/tests?{FILTER}=test:has:fast&{FILTER}=test:nohas:forms'))
        self.assertEqual(self.chip_count(section, 'filter'), 2)
        self.assertEqual(self.chip_field(section, FILTER, 0, 'column'), 'test')
        self.assertEqual(self.chip_field(section, FILTER, 0, 'op'), 'has')
        self.assertEqual(self.chip_field(section, FILTER, 0, 'value'), 'fast')
        self.assertEqual(self.chip_field(section, FILTER, 1, 'column'), 'test')
        self.assertEqual(self.chip_field(section, FILTER, 1, 'op'), 'nohas')
        self.assertEqual(self.chip_field(section, FILTER, 1, 'value'), 'forms')

    def test_a_filter_chip_offers_every_filterable_columns_label_and_that_columns_operator_labels(
        self,
    ) -> None:
        section = self.convicted_section(self.page(f'/tests?{FILTER}=test:has:editing'))
        column_select = re.search(r'<select name="f\.tests:0:column"[^>]*>(.*?)</select>',
                                  section, re.S).group(1)
        for name, column in filters.TESTS.columns.items():
            if column.filterable:
                self.assertIn(f'<option value="{name}"', column_select)
                self.assertIn(column.label, column_select)
        operator_select = re.search(r'<select name="f\.tests:0:op"[^>]*>(.*?)</select>',
                                    section, re.S).group(1)
        for operator in filters.TESTS.column('test').operators.values():
            self.assertIn(f'<option value="{operator.name}"', operator_select)
            self.assertIn(operator.label, operator_select)

    def test_a_column_with_a_vocabulary_renders_a_value_select_and_one_without_renders_an_input(
        self,
    ) -> None:
        vocabulary_section = self.convicted_section(
            self.page(f'/tests?{FILTER}=rule:anyof:{config.CLEAN_TREE}'))
        self.assertIsNotNone(re.search(r'<select name="f\.tests:0:value"', vocabulary_section))
        text_section = self.convicted_section(self.page(f'/tests?{FILTER}=test:has:editing'))
        self.assertIsNotNone(re.search(r'<input type="text" name="f\.tests:0:value"',
                                       text_section))

    def test_a_many_value_operator_on_a_vocabulary_column_renders_a_multiple_select(self) -> None:
        """`anyof`/`allof`/`oneof`/`exactly`/`noneof` all take many values, so a `<select>` with no
        `multiple` can only ever submit the one option a browser falls back to — which is why an
        `anyof` filter with two values used to come back empty."""
        value = f'{config.CLEAN_TREE}{filters.LIST_SEPARATOR}{config.DIRTY_TREE}'
        section = self.convicted_section(self.page(f'/tests?{FILTER}=rule:anyof:{value}'))
        select = re.search(r'<select name="f\.tests:0:value"([^>]*)>(.*?)</select>', section, re.S)
        self.assertIsNotNone(select)
        self.assertIn('multiple', select.group(1))
        self.assertIn(f'<option value="{config.CLEAN_TREE}" selected', select.group(2))
        self.assertIn(f'<option value="{config.DIRTY_TREE}" selected', select.group(2))
        self.assertIsNotNone(re.search(r'aria-label="[^"]+"', select.group(1)))

    def test_a_one_value_operator_on_a_vocabulary_column_still_renders_a_single_select(self) -> None:
        section = self.convicted_section(self.page(f'/tests?{FILTER}=suite:eq:layout-tests'))
        select = re.search(r'<select name="f\.tests:0:value"([^>]*)>', section)
        self.assertIsNotNone(select)
        self.assertNotIn('multiple', select.group(1))

    def test_the_sort_row_renders_a_column_select_and_a_direction_select(self) -> None:
        section = self.convicted_section(self.page(f'/tests?{SORT}=convictions:desc'))
        self.assertEqual(self.chip_field(section, SORT, 0, 'column'), 'convictions')
        self.assertEqual(self.chip_field(section, SORT, 0, 'direction'), 'desc')
        self.assertIsNotNone(re.search(r'<select name="s\.tests:0:direction"', section))

    def test_the_retired_free_text_filter_input_is_gone(self) -> None:
        section = self.convicted_section(self.page('/tests'))
        self.assertNotIn('<input type="search"', section)

    def test_the_surface_is_a_disclosure_in_the_table_header_that_opens_when_it_is_narrowing(self) -> None:
        """A details rather than an overlay: the panes clip their overflow and there is no script to
        give a positioned dialog an escape key."""
        section = self.convicted_section(self.page(f'/tests?{FILTER}=test:has:editing'))
        self.assertIn('<details class="table-filter" open>', section)
        self.assertIn('<details class="table-filter">',
                      self.convicted_section(self.page('/tests')))

    def test_the_disclosure_summary_carries_an_accessible_name(self) -> None:
        """The summary shows only a glyph, so a screen reader has nothing else to read it as."""
        section = self.convicted_section(self.page('/tests'))
        self.assertIn('aria-label="Filter and sort"', section)

    def test_the_summary_shows_the_active_filter_count_only_when_narrowing(self) -> None:
        unfiltered = self.convicted_section(self.page('/tests'))
        self.assertNotIn('class="active"', unfiltered)
        narrowed = self.convicted_section(self.page(f'/tests?{FILTER}=test:has:editing'))
        summary = re.search(r'<summary[^>]*>(.*?)</summary>', narrowed, re.S)
        self.assertIsNotNone(summary)
        self.assertIn('class="active"', summary.group(0))
        self.assertIn('1', summary.group(1))

    def test_a_filter_offers_a_way_back_to_the_unfiltered_list(self) -> None:
        page = self.page(f'/tests?{FILTER}=test:has:editing')
        cleared = re.search(r'<a class="badge" href="([^"]*)">clear</a>', page)
        self.assertIsNotNone(cleared)
        self.assertNotIn(FILTER, cleared.group(1))
        self.assertIn('fast/forms/input.html', self.page(cleared.group(1).replace('&amp;', '&')))

    def test_a_filter_that_matches_nothing_says_so_rather_than_reading_as_an_empty_table(self) -> None:
        section = self.convicted_section(self.page(f'/tests?{FILTER}=test:has:nomatch'))
        self.assertIn('No convicted test matches test:has:nomatch.', section)
        self.assertNotIn('Nothing convicted in this window.', section)

    def test_an_off_filter_alone_reads_as_unfiltered_everywhere_the_page_says_so(self) -> None:
        """`off` is a kept chip with no SQL behind it, so the header sentence, the placeholder and
        the clear badge must all read the page as unfiltered, not as narrowed to nothing."""
        section = self.convicted_section(self.page(f'/tests?{FILTER}=rule:off'))
        self.assertIn('All 2 convicted tests.', section)
        self.assertNotIn('matching rule:off', section)
        self.assertNotIn('<a class="badge" href="', section.split('<details', 1)[0])

    def test_an_off_filter_alongside_a_real_one_still_narrows(self) -> None:
        section = self.convicted_section(
            self.page(f'/tests?{FILTER}=rule:off&{FILTER}=test:has:editing'))
        self.assertIn('matching rule:off and test:has:editing', section)

    def test_a_filter_survives_a_sort_header_click(self) -> None:
        page = self.page(f'/tests?{FILTER}=test:has:editing')
        clicked = self.page(self.sort_link(page, 'test'))
        self.assertIn('editing/pasteboard/copy.html', clicked)
        self.assertNotIn('fast/forms/input.html', clicked)

    def test_a_filter_reports_a_cap_that_cut_the_matching_set(self) -> None:
        for number in range(2, 6):
            self.store_build(number + 10,
                             flaky={f'editing/case{number:03d}.html': config.CLEAN_TREE})
        with mock.patch('ews_dashboard.web.app.convictions.convicted_tests',
                        functools.partial(convictions.convicted_tests, limit=2)):
            page = self.page(f'/tests?{FILTER}=test:has:editing')
        section = self.convicted_section(page)
        self.assertIn('Showing the first 2 of 5 tests matching test:has:editing.', section)


class TestTestsChipForm(WebTest):
    """The chip form submits the exploded per-control grammar; the route turns that back into the
    canonical `f./s.` spelling and redirects, so every link the page emits still speaks one grammar."""

    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={'editing/pasteboard/copy.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/forms/input.html': config.CLEAN_TREE})

    def chip_query(self, index: int, column: str, op: str, value: str) -> str:
        return (f'{filters.filter_chip_argument(filters.TESTS, index, "column")}={column}'
                f'&{filters.filter_chip_argument(filters.TESTS, index, "op")}={op}'
                f'&{filters.filter_chip_argument(filters.TESTS, index, "value")}={value}')

    def test_an_exploded_submission_redirects_to_the_canonical_url(self) -> None:
        response = self.client.get(f'/tests?{self.chip_query(0, "test", "has", "editing")}')
        self.assertEqual(response.status_code, 302)
        target = urlsplit(response.headers['Location'])
        self.assertEqual(parse_qs(target.query).get(FILTER), ['test:has:editing'])

    def test_the_redirect_keeps_every_other_query_argument(self) -> None:
        build_id = self.store_build(3, flaky={'editing/pasteboard/copy.html': config.CLEAN_TREE})
        query = (f'days=14&suite=layout-tests&builder={fixtures.LAYOUT_BUILDER}'
                 f'&build={build_id}&test=copy&state={formatting.ANY_STATE}'
                 f'&min_shown=1&max_shown=9&page=2'
                 f'&{self.chip_query(0, "test", "has", "editing")}')
        response = self.client.get(f'/tests?{query}')
        self.assertEqual(response.status_code, 302)
        held = parse_qs(urlsplit(response.headers['Location']).query)
        self.assertEqual(held['days'], ['14'])
        self.assertEqual(held['suite'], ['layout-tests'])
        self.assertEqual(held['builder'], [fixtures.LAYOUT_BUILDER])
        self.assertEqual(held['build'], [str(build_id)])
        self.assertEqual(held['test'], ['copy'])
        self.assertEqual(held['state'], [formatting.ANY_STATE])
        self.assertEqual(held['min_shown'], ['1'])
        self.assertEqual(held['max_shown'], ['9'])
        self.assertEqual(held['page'], ['2'])

    def test_following_the_redirect_narrows_exactly_as_the_canonical_url_does(self) -> None:
        response = self.client.get(f'/tests?{self.chip_query(0, "test", "has", "editing")}')
        followed = self.page(response.headers['Location'])
        canonical = self.page(f'/tests?{FILTER}=test:has:editing')
        self.assertEqual(self.convicted_section(followed), self.convicted_section(canonical))

    def test_adding_a_filter_chip_renders_one_more_blank_chip_and_narrows_nothing(self) -> None:
        page = self.page(f'/tests?{web_app.ADD_FILTER_ARGUMENT}=1')
        self.assertIn('editing/pasteboard/copy.html', page)
        self.assertIn('fast/forms/input.html', page)
        section = self.convicted_section(page)
        row = re.search(r'data-chip-kind="filter"[^>]*>(.*?)<button type="submit" '
                        r'name="add_filter"', section, re.S)
        self.assertEqual(row.group(1).count('<span class="chip">'), 1)

    def test_adding_a_sort_chip_renders_one_more_blank_chip_and_narrows_nothing(self) -> None:
        page = self.page(f'/tests?{web_app.ADD_SORT_ARGUMENT}=1')
        section = self.convicted_section(page)
        row = re.search(r'data-chip-kind="sort"[^>]*>(.*?)<button type="submit" '
                        r'name="add_sort"', section, re.S)
        self.assertEqual(row.group(1).count('<span class="chip">'), 1)

    def test_a_canonical_url_with_no_exploded_arguments_is_not_redirected(self) -> None:
        response = self.client.get(f'/tests?{FILTER}=test:has:editing')
        self.assertEqual(response.status_code, 200)

    def test_a_two_value_set_filter_round_trips_through_the_exploded_form_and_back(self) -> None:
        """A `<select multiple>` submits its own field once per selected option, which is exactly
        what a chip form does with no script at all — the Flask test client runs none — so this is
        the no-JavaScript proof that an `anyof` filter with two values survives its own form."""
        query = (f'{filters.filter_chip_argument(filters.TESTS, 0, "column")}=rule'
                 f'&{filters.filter_chip_argument(filters.TESTS, 0, "op")}=anyof'
                 f'&{filters.filter_chip_argument(filters.TESTS, 0, "value")}={config.CLEAN_TREE}'
                 f'&{filters.filter_chip_argument(filters.TESTS, 0, "value")}={config.DIRTY_TREE}')
        response = self.client.get(f'/tests?{query}')
        self.assertEqual(response.status_code, 302)
        canonical = f'rule:anyof:{config.CLEAN_TREE}{filters.LIST_SEPARATOR}{config.DIRTY_TREE}'
        target = urlsplit(response.headers['Location'])
        self.assertEqual(parse_qs(target.query).get(FILTER), [canonical])


class TestTestsUnderscoreArguments(WebTest):
    """A query argument spelled like one of `url_for`'s own keywords (`_scheme`, `_anchor`,
    `_external`, `_method`) must never reach it: those are keyword-only there, three of them take a
    single value while every argument this page carries forward is a list, and `_external` would
    otherwise succeed while quietly turning every in-page link on the page absolute."""

    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})

    def test_a_scheme_named_argument_does_not_crash_the_page(self) -> None:
        self.page('/tests?_scheme=http')

    def test_an_anchor_named_argument_does_not_crash_the_page(self) -> None:
        self.page('/tests?_anchor=x')

    def test_a_method_named_argument_does_not_crash_the_page(self) -> None:
        self.page('/tests?_method=POST')

    def test_an_external_named_argument_does_not_leak_into_the_pages_own_links(self) -> None:
        page = self.page('/tests?_external=1')
        link = self.sort_link(page, 'test')
        self.assertTrue(link.startswith('/tests'), link)
        self.assertNotIn('_external', link)

    def test_an_underscore_argument_survives_alongside_a_real_filter(self) -> None:
        page = self.page(f'/tests?{FILTER}=test:has:fast&_scheme=http')
        self.assertIn('fast/a.html', page)

    def test_an_underscore_argument_alongside_a_chip_submission_does_not_crash_the_redirect(
        self,
    ) -> None:
        """The redirect a chip-form submission takes builds its own carried-argument dict, which is
        the path that used to raise `TypeError: unhashable type: 'list'` once a reader-named argument
        collided with one of `url_for`'s own."""
        query = (f'{filters.filter_chip_argument(filters.TESTS, 0, "column")}=test'
                 f'&{filters.filter_chip_argument(filters.TESTS, 0, "op")}=has'
                 f'&{filters.filter_chip_argument(filters.TESTS, 0, "value")}=fast'
                 f'&_scheme=http')
        response = self.client.get(f'/tests?{query}')
        self.assertEqual(response.status_code, 302)
        self.assertNotIn('_scheme', response.headers['Location'])


class TestTestsUnreadableFilters(WebTest):
    """A clause this page cannot read is dropped and reported, never obeyed as something else and
    never a refused request: the reader has to be able to tell a filter that matched everything from
    a filter that was thrown away."""

    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={'editing/pasteboard/copy.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/forms/input.html': config.CLEAN_TREE})

    def test_an_unreadable_filter_renders_the_page_and_names_what_it_ignored(self) -> None:
        page = self.page(f'/tests?{FILTER}=nonsense:eq:1')
        self.assertIn('Ignored 1 filter this page cannot read: nonsense:eq:1.', page)
        self.assertIn('editing/pasteboard/copy.html', page)
        self.assertIn('fast/forms/input.html', page)

    def test_an_unreadable_filter_is_reported_once_and_not_once_per_section(self) -> None:
        self.assertEqual(self.page(f'/tests?{FILTER}=nonsense:eq:1').count('Ignored 1 filter'), 1)

    def test_the_clauses_that_do_parse_still_apply_and_the_ignored_one_is_not_carried_on(self) -> None:
        page = self.page(f'/tests?{FILTER}=nonsense:eq:1&{FILTER}=test:has:editing')
        self.assertIn('Ignored 1 filter', page)
        self.assertNotIn('fast/forms/input.html', page)
        self.assertNotIn('nonsense', self.sort_link(page, 'test'))

    def test_an_operator_of_the_wrong_kind_and_a_value_of_the_wrong_type_are_both_ignored(self) -> None:
        page = self.page(f'/tests?{FILTER}=test:gt:editing&{FILTER}=convictions:ge:several')
        self.assertIn('Ignored 2 filters this page cannot read: '
                      'test:gt:editing, convictions:ge:several.', page)
        self.assertIn('fast/forms/input.html', page)

    def test_a_clause_with_no_operator_at_all_is_ignored(self) -> None:
        self.assertIn('Ignored 1 filter this page cannot read: test.',
                      self.page(f'/tests?{FILTER}=test'))

    def test_the_empty_field_a_form_always_offers_is_not_a_mistake_a_reader_made(self) -> None:
        """Submitting the surface sends an empty clause every time, and telling a reader they got
        something wrong for using the form would be the report crying wolf."""
        page = self.page(f'/tests?{FILTER}=&{FILTER}=test:has:editing')
        self.assertNotIn('cannot read', page)
        self.assertNotIn('fast/forms/input.html', page)


class TestTestsRetiredArguments(WebTest):
    """The old ad-hoc grammar is gone rather than aliased. Nothing bookmarked it outside this repo,
    and an alias would have left two spellings of one state for every link on the page to keep."""

    def setUp(self) -> None:
        super().setUp()
        self.store_build(1, flaky={'editing/pasteboard/copy.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/forms/input.html': config.CLEAN_TREE})

    def test_find_no_longer_narrows_anything(self) -> None:
        page = self.page('/tests?find=editing')
        self.assertIn('fast/forms/input.html', page)
        self.assertIn('All 2 convicted tests.', self.convicted_section(page))

    def test_rule_no_longer_narrows_anything(self) -> None:
        page = self.page(f'/tests?rule={config.CLEAN_TREE}')
        self.assertIn('editing/pasteboard/copy.html', page)
        self.assertIn('fast/forms/input.html', page)
        self.assertIn('All 2 convicted tests.', self.convicted_section(page))

    def test_sort_and_dir_no_longer_order_anything(self) -> None:
        page = self.page('/tests?sort=test&dir=asc')
        self.assertLess(page.index('editing/pasteboard/copy.html'),
                        page.index('fast/forms/input.html'))
        self.assertEqual(set(TestTestsSorting.SORTED_HEADER.findall(page)), {'Convictions'})

    def test_a_retired_argument_is_not_carried_forward_by_the_pages_own_links(self) -> None:
        link = self.sort_link(self.page('/tests?find=editing&sort=test&dir=asc'), 'test')
        for retired in ('find=', 'sort=', 'dir='):
            self.assertNotIn(retired, link)


class TestTestsPageMove(WebTest):
    """The convicted-tests table moved off Explore onto its own page."""

    def test_tests_renders_the_convicted_table_and_explore_no_longer_does(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.assertIn('id="convicted"', self.page('/tests'))
        self.assertNotIn('id="convicted"', self.page('/explore'))


class TestTestsDrilldown(WebTest):
    """Clicking a convicted test opens a panel beside the table listing every conviction of it."""

    def test_the_drilldown_lists_every_conviction_of_the_named_test(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/a.html': config.DIRTY_TREE})
        page = self.page('/tests?test=fast%2Fa.html')
        self.assertIn('id="test-detail"', page)
        detail = self.detail_pane(page)
        self.assertEqual(detail.count('#1'), 1)
        self.assertEqual(detail.count('#2'), 1)
        self.assertIn(config.CLEAN_TREE, detail)
        self.assertIn(config.DIRTY_TREE, detail)

    def test_a_test_with_no_convictions_reports_nothing_rather_than_an_empty_table(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        page = self.page('/tests?test=fast%2Fb.html')
        self.assertIn('No convictions of', page)
        self.assertNotIn('<table', self.detail_pane(page))

    def test_clicking_a_test_name_opens_its_drilldown_and_keeps_the_other_arguments(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        page = self.page('/tests?days=14')
        found = re.search(r'<a href="([^"]*test=fast/a\.html[^"]*)">fast/a\.html</a>', page)
        self.assertIsNotNone(found, 'no link opened the drilldown')
        self.assertIn('days=14', found.group(1).replace('&amp;', '&'))

    def test_the_pane_title_links_to_the_tests_investigation_across_every_configuration(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/a.html': config.DIRTY_TREE})
        page = self.page('/tests?test=fast%2Fa.html')
        detail = self.detail_pane(page)
        found = re.search(r'<div class="title"><a href="([^"]*)">fast/a\.html</a></div>', detail)
        self.assertIsNotNone(found, 'the pane title is not a link to the test investigation')
        parameters = parse_qs(urlsplit(found.group(1).replace('&amp;', '&')).query)
        self.assertEqual(parameters, {'suite': ['layout-tests'], 'test': ['fast/a.html']})

    def test_the_drilldown_table_sits_in_a_scroll_container(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        page = self.page('/tests?test=fast%2Fa.html')
        detail = self.detail_pane(page)
        self.assertRegex(detail, r'<div class="table-scroll"[^>]*>\s*<table')


class TestTestsDrilldownCap(WebTest):
    """The drilldown has the same hard cap as the convicted-tests table, and names how many of how
    many when the cap cuts the set."""

    TEST_NAME = 'fast/drilled.html'

    def _convict_many(self, count: int) -> None:
        for number in range(1, count + 1):
            self.store_build(number, flaky={self.TEST_NAME: config.CLEAN_TREE})

    def _capped(self, limit: int) -> mock._patch:
        """Shrinks the drilldown's row cap for one request, so a truncation test does not need
        hundreds of builds to trigger it."""
        return mock.patch('ews_dashboard.web.app.convictions.test_convictions',
                          functools.partial(convictions.test_convictions, limit=limit))

    def test_the_cap_bounds_the_rows_rendered(self) -> None:
        self._convict_many(5)
        with self._capped(3):
            page = self.page(f'/tests?test={quote(self.TEST_NAME)}')
        detail = self.detail_pane(page)
        self.assertEqual(detail.count('<tr>\n'), 3)

    def test_the_pane_names_how_many_of_how_many_when_the_cap_cuts_the_set(self) -> None:
        self._convict_many(5)
        with self._capped(3):
            page = self.page(f'/tests?test={quote(self.TEST_NAME)}')
        detail = self.detail_pane(page)
        self.assertIn('Showing the first 3 of 5 convictions.', detail)

    def test_a_set_within_the_cap_reports_no_truncation_notice(self) -> None:
        self._convict_many(2)
        page = self.page(f'/tests?test={quote(self.TEST_NAME)}')
        detail = self.detail_pane(page)
        self.assertNotIn('Showing the first', detail)

    def test_the_pane_title_is_not_a_link_when_the_test_has_no_convictions_to_read_a_configuration_from(
        self,
    ) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        page = self.page('/tests?test=fast%2Fb.html')
        detail = self.detail_pane(page)
        self.assertIn('<div class="title">fast/b.html</div>', detail)


class TestExploreDrillDown(WebTest):
    """The builds pane and the pane for one selected build."""

    def cache_history(self, build_id: int, test_name: str, pass_pct: float) -> None:
        """Write the history row a refresh would have left for one of a build's surfaced tests, so
        the detail pane has an answer to read without a fetch of its own."""
        build_row = self.stored_build(build_id)
        self.cache_answer(
            results.Query(test_name, results.Configuration.of_build(build_row),
                          false_positive.base_commit_of(build_row) or ''),
            {'pass': pass_pct, 'fail': 100.0 - pass_pct, 'timeout': 0.0, 'crash': 0.0,
             'image': 0.0, 'audio': 0.0, 'text': 0.0, 'error': 0.0, 'warning': 0.0},
        )

    def verdict_span(self, verdict: str) -> str:
        """One verdict as the page shows it: the class it is styled by and the word it reads as.
        Spelled out rather than asked of `formatting`, so a filter that stopped distinguishing
        states cannot make its own expectation."""
        return f'<span class="state state-{verdict}">{formatting.VERDICT_WORDS[verdict]}</span>'

    def test_a_selected_build_lists_its_surfaced_tests_with_the_word_each_verdict_reads_as(self) -> None:
        build_id = self.store_build(1, first=['fast/pre.html', 'fast/real.html'],
                                    second=['fast/pre.html', 'fast/real.html'], clean=[])
        self.cache_history(build_id, 'fast/pre.html', UNRELIABLE)
        self.cache_history(build_id, 'fast/real.html', RELIABLE)
        self.classify_everything({'fast/pre.html': UNRELIABLE, 'fast/real.html': RELIABLE})
        page = self.page(f'/explore?build={build_id}')
        self.assertIn('fast/pre.html', page)
        self.assertIn('fast/real.html', page)
        self.assertIn(self.verdict_span(false_positive.PRE_EXISTING), page)
        self.assertIn(self.verdict_span(false_positive.REAL), page)

    def test_the_selected_build_is_marked_selected_in_the_builds_pane_and_another_build_is_not(self) -> None:
        """`state=all` keeps this about selection, not about a build the default state filter hides."""
        selected = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        other = self.store_build(2, first=['fast/b.html'], second=['fast/b.html'], clean=[])
        page = self.page(f'/explore?build={selected}&state={formatting.ANY_STATE}')
        self.assertIn('selected', self.entry_classes(page, selected))
        self.assertNotIn('selected', self.entry_classes(page, other))

    def test_a_state_that_blames_noise_marks_its_builds_pane_entry_and_a_clean_one_does_not(self) -> None:
        noise = self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[])
        clean = self.store_build(2, first=['fast/real.html'], second=['fast/real.html'], clean=[])
        self.classify_everything({'fast/pre.html': UNRELIABLE, 'fast/real.html': RELIABLE})
        page = self.page(f'/explore?state={formatting.ANY_STATE}')
        self.assertIn('noise', self.entry_classes(page, noise))
        self.assertNotIn('noise', self.entry_classes(page, clean))

    def test_a_build_id_that_is_unknown_or_not_a_number_prompts_for_a_build_rather_than_raising(self) -> None:
        prompt = 'Pick a build to see the tests it showed its author'
        self.assertIn(prompt, self.page('/explore?build=999999'))
        self.assertIn(prompt, self.page('/explore?build=nonsense'))

    def test_a_test_filter_shows_only_the_surfaced_tests_whose_names_match_it(self) -> None:
        build_id = self.store_build(1, first=['fast/kept.html', 'fast/hidden.html'],
                                    second=['fast/kept.html', 'fast/hidden.html'], clean=[])
        page = self.page(f'/explore?build={build_id}&test=kept')
        self.assertIn('fast/kept.html', page)
        self.assertNotIn('fast/hidden.html', page)

    def test_a_test_filter_that_matches_nothing_says_so_rather_than_reading_as_an_empty_build(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        page = self.page(f'/explore?build={build_id}&test=nomatch')
        self.assertIn('None of the 1 tests this build showed its author match', page)
        self.assertNotIn('This build showed its author no new failures.', page)

    def test_a_build_whose_history_cannot_be_believed_says_why_and_reads_as_not_looked_up(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[],
                                    identifier=None)
        page = self.page(f'/explore?build={build_id}')
        self.assertIn(false_positive.REASON_DESCRIPTIONS[false_positive.NO_BASE_COMMIT], page)
        self.assertIn(self.verdict_span(false_positive.UNQUERIED), page)

    def test_a_failing_build_no_refresh_has_classified_reads_as_unclassified_not_as_clean(self) -> None:
        """Scoped to the detail pane rather than the whole page, since the legend at the foot of the
        page renders a CLEAN chip of its own as one of the vocabulary entries it explains; that chip
        is not this build's own state and must not make this assertion pass or fail on its account."""
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        page = self.page(f'/explore?build={build_id}')
        detail = page[page.index('<div class="pane detail">'):page.index('<div class="text tiny caption">')]
        self.assertIn('<span class="state state-unknown">unclassified</span>', detail)
        self.assertIn('No refresh has classified this build yet, so nothing below has been looked '
                      'up.', detail)
        self.assertNotIn(f'<span class="state state-{false_positive.CLEAN}">', detail)

    def test_the_builds_pane_discloses_how_many_failing_builds_it_is_not_showing(self) -> None:
        for number in (1, 2, 3):
            self.store_build(number, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        with mock.patch.object(web_app, 'BUILDS_SHOWN', 2):
            page = self.page(f'/explore?state={formatting.ANY_STATE}')
            self.assertIn('<span class="tally">2 of 3</span>', page)

    def test_a_build_that_surfaced_nothing_to_its_author_says_so_in_place_of_a_table(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=[], clean=[])
        self.assertIn('This build showed its author no new failures.',
                      self.page(f'/explore?build={build_id}'))

    def test_a_classified_build_that_surfaced_nothing_reads_as_nothing_shown_not_unclassified(self) -> None:
        """It has no bucket and nothing to refresh, so reading as unclassified would send a reader
        to a refresh that would change nothing."""
        build_id = self.store_build(1, first=['fast/a.html'], second=[], clean=[])
        self.classify_everything({})
        page = self.page(f'/explore?build={build_id}')
        self.assertIn(f'<span class="state state-{formatting.NO_SURFACED}">'
                      f'{formatting.BUCKET_WORDS[formatting.NO_SURFACED]}</span>', page)
        self.assertNotIn('<span class="state state-unknown">unclassified</span>', page)


class TestExploreBuildFilter(WebTest):
    """The builds pane's own filter: a details and a GET form, since the app needs no JavaScript and
    a narrowed pane has to be a URL a reader can link and go back from."""

    def listed_builds(self, page: str) -> list:
        """The builds the pane listed, as the ids its entries link to."""
        return re.findall(r'<a class="entry[^"]*" href="[^"]*build=(\d+)', self.builds_pane(page))

    def entry_link(self, page: str, pattern: str) -> str:
        found = re.search(rf'<a class="entry[^"]*" href="([^"]*{re.escape(pattern)}[^"]*)"', page)
        self.assertIsNotNone(found, f'the page rendered no entry matching {pattern}')
        return found.group(1).replace('&amp;', '&')

    def three_states(self) -> dict:
        """One build in each of three states, and three sizes with them: all noise showing one test,
        a real failure showing one, and a partly-noise build showing three."""
        builds = {
            'noise': self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'],
                                      clean=[]),
            'real': self.store_build(2, first=['fast/real.html'], second=['fast/real.html'],
                                     clean=[]),
            'mixed': self.store_build(3, first=['fast/pre.html', 'fast/other.html',
                                                'fast/real.html'],
                                      second=['fast/pre.html', 'fast/other.html',
                                              'fast/real.html'],
                                      clean=[]),
        }
        self.classify_everything({'fast/pre.html': UNRELIABLE, 'fast/other.html': UNRELIABLE,
                                 'fast/real.html': RELIABLE})
        return builds

    def test_a_state_filter_lists_only_the_builds_in_that_state(self) -> None:
        builds = self.three_states()
        self.assertEqual(self.listed_builds(self.page(f'/explore?state={false_positive.FALSE_RED}')),
                         [str(builds['noise'])])

    def test_two_states_asked_for_together_list_the_builds_in_either_of_them(self) -> None:
        builds = self.three_states()
        page = self.page(f'/explore?state={false_positive.FALSE_RED}'
                         f'&state={false_positive.PARTIAL_FP}')
        self.assertEqual(sorted(self.listed_builds(page)),
                         sorted((str(builds['noise']), str(builds['mixed']))))

    def test_a_build_no_refresh_has_reached_is_filterable_as_unclassified(self) -> None:
        self.three_states()
        unreached = self.store_build(4, first=['fast/new.html'], second=['fast/new.html'], clean=[])
        self.assertEqual(self.listed_builds(self.page(f'/explore?state={formatting.UNCLASSIFIED}')),
                         [str(unreached)])

    def test_a_range_keeps_the_builds_sitting_exactly_on_either_bound(self) -> None:
        """Bounds are exercised past the default two noise states, via `state=all`, so a real
        failure excluded by the default cannot be mistaken for one excluded by the range."""
        builds = self.three_states()
        any_state = f'state={formatting.ANY_STATE}'
        small = sorted((str(builds['noise']), str(builds['real'])))
        self.assertEqual(sorted(self.listed_builds(self.page(f'/explore?{any_state}&min_shown=1&max_shown=1'))),
                         small)
        self.assertEqual(self.listed_builds(self.page(f'/explore?{any_state}&min_shown=3')),
                         [str(builds['mixed'])])
        self.assertEqual(sorted(self.listed_builds(self.page(f'/explore?{any_state}&min_shown=1&max_shown=3'))),
                         sorted(str(each) for each in builds.values()))

    def test_a_state_that_is_not_a_state_is_ignored_rather_than_emptying_the_pane(self) -> None:
        """A state list with no recognized value at all falls back to the default two noise states
        rather than to every state, so a hand-mangled URL cannot silently widen the pane."""
        builds = self.three_states()
        self.assertEqual(sorted(self.listed_builds(self.page('/explore?state=NotAState'))),
                         sorted((str(builds['noise']), str(builds['mixed']))))

    def test_a_bound_that_is_not_a_number_is_ignored_rather_than_raising(self) -> None:
        builds = self.three_states()
        self.assertEqual(sorted(self.listed_builds(
            self.page(f'/explore?state={formatting.ANY_STATE}&min_shown=&max_shown=lots'))),
            sorted(str(each) for each in builds.values()))

    def test_a_minimum_above_the_maximum_narrows_to_nothing_rather_than_failing(self) -> None:
        self.three_states()
        page = self.page(f'/explore?state={formatting.ANY_STATE}&min_shown=3&max_shown=1')
        self.assertEqual(self.listed_builds(page), [])
        self.assertIn('No failing builds in this window match this filter.', page)

    def test_a_narrowed_pane_counts_against_what_matched_not_every_failing_build(self) -> None:
        self.three_states()
        self.assertIn('<span class="tally">1 shown of 1 matching</span>',
                      self.page(f'/explore?state={false_positive.FALSE_RED}'))

    def test_an_unnarrowed_pane_still_counts_against_every_failing_build(self) -> None:
        """`state=all` is what makes the pane unnarrowed now that no `state` argument at all defaults
        to the two noise states."""
        self.three_states()
        with mock.patch.object(web_app, 'BUILDS_SHOWN', 2):
            self.assertIn('<span class="tally">2 of 3</span>',
                          self.page(f'/explore?state={formatting.ANY_STATE}'))

    def test_a_narrowed_pane_says_how_many_matched_beyond_the_page_it_shows(self) -> None:
        self.three_states()
        with mock.patch.object(web_app, 'BUILDS_SHOWN', 2):
            page = self.page(f'/explore?state={formatting.ANY_STATE}&min_shown=1')
        self.assertEqual(len(self.listed_builds(page)), 2)
        self.assertIn('<span class="tally">2 shown of 3 matching</span>', page)

    def test_a_matching_build_older_than_the_page_is_still_listed(self) -> None:
        """Narrowing the page instead of the window read as no build in 90 days having been all
        noise when the only build that was is older than the newest `BUILDS_SHOWN`."""
        now = int(time.time())
        noise = self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[],
                                 started_at=now - 4 * 3600)
        for number in (2, 3):
            self.store_build(number, first=['fast/real.html'], second=['fast/real.html'], clean=[],
                             started_at=now - number * 600)
        self.classify_everything({'fast/pre.html': UNRELIABLE, 'fast/real.html': RELIABLE})
        with mock.patch.object(web_app, 'BUILDS_SHOWN', 2):
            page = self.page(f'/explore?state={false_positive.FALSE_RED}')
        self.assertEqual(self.listed_builds(page), [str(noise)])
        self.assertIn('<span class="tally">1 shown of 1 matching</span>', page)
        self.assertNotIn('No failing builds in this window match this filter.', page)

    def test_the_filter_travels_with_the_links_the_page_renders_and_holds_when_one_is_followed(self) -> None:
        builds = self.three_states()
        page = self.page(f'/explore?state={false_positive.FALSE_RED}&min_shown=1&max_shown=2')
        build_link = self.entry_link(self.builds_pane(page), f'build={builds["noise"]}')
        self.assertIn(f'state={false_positive.FALSE_RED}', build_link)
        self.assertIn('min_shown=1', build_link)
        self.assertIn('max_shown=2', build_link)
        self.assertEqual(self.listed_builds(self.page(build_link)), [str(builds['noise'])])

    def test_the_queue_picker_form_also_holds_the_builds_pane_filter(self) -> None:
        """The queue picker's form is not the builds pane's own filter form, so unlike it, it never
        adds the build filter's fields itself -- it has to hold them the same way it holds `days` and
        `suite`, or picking a queue would silently drop whatever state/min_shown/max_shown the reader
        had set."""
        self.three_states()
        page = self.page(f'/explore?state={false_positive.FALSE_RED}&min_shown=1&max_shown=2')
        tree_form = re.search(r'<details class="table-filter queue-tree".*?</form>\s*</details>', page, re.S)
        self.assertIsNotNone(tree_form, 'no queue picker form on the page')
        form = tree_form.group(0)
        self.assertIn(f'<input type="hidden" name="state" value="{false_positive.FALSE_RED}">', form)
        self.assertIn('<input type="hidden" name="min_shown" value="1">', form)
        self.assertIn('<input type="hidden" name="max_shown" value="2">', form)

    def test_the_queue_picker_form_holds_no_hidden_copy_of_the_queue_selection(self) -> None:
        """The picker's own checkboxes are the queue selection, so its form cannot also carry a
        hidden `group`/`version`/`builder`: a hidden copy is what let an unticked checkbox keep
        submitting the old value, since the browser sends nothing for an unticked box but a hidden
        input beside it still would."""
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, builder=fixtures.GTK_BUILDER,
                         builder_id=9)
        page = self.page('/explore?group=GTK')
        tree_form = re.search(r'<details class="table-filter queue-tree".*?</form>\s*</details>', page, re.S)
        self.assertIsNotNone(tree_form, 'no queue picker form on the page')
        form = tree_form.group(0)
        self.assertNotRegex(form, r'<input type="hidden" name="group"')
        self.assertNotRegex(form, r'<input type="hidden" name="version"')
        self.assertNotRegex(form, r'<input type="hidden" name="builder"')

    def test_unticking_one_queue_checkbox_would_leave_a_smaller_selection(self) -> None:
        """The behavioural version of the same bug: with two queues selected, unticking one box's
        own checkbox has to leave the picker's form submitting fewer queues than it holds now, or
        the hidden fields beside the checkboxes are quietly putting the old value back."""
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, builder=fixtures.GTK_BUILDER,
                         builder_id=9)
        self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, builder=fixtures.WPE_BUILDER,
                         builder_id=10)
        page = self.page('/explore?group=GTK&group=WPE')
        tree_form = re.search(r'<details class="table-filter queue-tree".*?</form>\s*</details>', page, re.S)
        self.assertIsNotNone(tree_form, 'no queue picker form on the page')
        form = tree_form.group(0)
        checked = set(re.findall(r'<input type="checkbox" name="group" value="([^"]*)"[^>]* checked>', form))
        hidden = set(re.findall(r'<input type="hidden" name="group" value="([^"]*)">', form))
        self.assertEqual(checked, {'GTK', 'WPE'})
        submitted_after_unticking_gtk = (checked - {'GTK'}) | hidden
        self.assertLess(len(submitted_after_unticking_gtk), len(checked))

    def test_a_chosen_state_comes_back_checked_so_the_dropdown_reads_as_what_it_did(self) -> None:
        self.three_states()
        page = self.page(f'/explore?state={false_positive.FALSE_RED}')
        self.assertIn(f'<input type="checkbox" name="state" value="{false_positive.FALSE_RED}"'
                      ' checked>', page)
        self.assertIn(f'<input type="checkbox" name="state" value="{false_positive.CLEAN}">', page)

    def test_no_state_argument_defaults_to_the_two_noise_states(self) -> None:
        builds = self.three_states()
        self.assertEqual(sorted(self.listed_builds(self.page('/explore'))),
                         sorted((str(builds['noise']), str(builds['mixed']))))

    def test_state_all_shows_every_state(self) -> None:
        builds = self.three_states()
        self.assertEqual(sorted(self.listed_builds(self.page(f'/explore?state={formatting.ANY_STATE}'))),
                         sorted(str(each) for each in builds.values()))

    def test_the_default_state_filter_does_not_force_the_pane_open(self) -> None:
        self.three_states()
        self.assertNotIn('<details class="builds-filter" open>', self.page('/explore'))

    def test_a_state_asked_for_by_hand_forces_the_pane_open(self) -> None:
        self.three_states()
        self.assertIn('<details class="builds-filter" open>',
                      self.page(f'/explore?state={false_positive.FALSE_RED}'))

    def test_the_clear_badge_returns_to_the_default_rather_than_to_every_state(self) -> None:
        builds = self.three_states()
        page = self.page(f'/explore?state={false_positive.FALSE_RED}&min_shown=1')
        cleared = self.chooser_link(page, 'clear')
        self.assertNotIn('state=', cleared)
        self.assertEqual(sorted(self.listed_builds(self.page(cleared))),
                         sorted((str(builds['noise']), str(builds['mixed']))))


class TestEscapes(WebTest):
    """The escape page, which shows what main did with a convicted test after the change landed."""

    def _stored_escape(self, build_id: int, test_name: str, verdict: str,
                       failed_after: int = 3, runs_after: int = 3,
                       recent_runs: Optional[int] = None, recent_failed: Optional[int] = None,
                       recent_checked_at: Optional[int] = None,
                       landed_at: Optional[int] = fixtures.DEFAULT_BUILD_TIME) -> None:
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, landed_at, window_ends_at, decided_at, recent_runs, recent_failed,
                    recent_checked_at
                ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)''',
                (build_id, test_name, verdict, 4, 0, runs_after, failed_after, landed_at,
                 int(time.time()), int(time.time()), recent_runs, recent_failed, recent_checked_at),
            )

    def selected_category(self, page: str) -> str:
        """The verdict the categories pane shows as chosen, read from the pill inside its entry. The
        pill is what tells a category entry from the queue rail's, which is selected too."""
        found = re.search(r'<a class="entry selected"[^>]*>\s*<span class="line">\s*'
                          r'<span class="label"><span class="state state-(\w+)">', page)
        self.assertIsNotNone(found, 'no category entry is selected')
        return found.group(1)

    def test_a_window_with_nothing_decided_says_so_rather_than_reading_as_no_escapes(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1, pr_title='One')
        page = self.page('/escapes')
        self.assertIn('No conviction in this window escaped', page)
        self.assertIn('1 not looked for', page)

    def test_an_escape_is_listed_with_the_runs_behind_it(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=72555,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        page = self.page('/escapes')
        self.assertIn('fast/a.html', page)
        self.assertIn('3 of 3', page)
        self.assertIn('pull/72555', page)

    def test_the_two_computed_columns_show_a_header_and_a_value(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=72555,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED, failed_after=1, runs_after=8,
                            recent_runs=40, recent_failed=3, recent_checked_at=int(time.time()))
        page = self.page('/escapes')
        self.assertIn('Escape strength', page)
        self.assertIn('Current damage', page)
        self.assertIn('2.8%', page)
        self.assertIn('1/8', page)
        self.assertIn('7.5%', page)
        self.assertIn('3/40', page)

    def test_the_computed_columns_round_to_one_decimal_place(self) -> None:
        """`percent` itself prints every significant digit sqlite's float division carries, which
        rendered a share like 1 of 311 as 0.321543% instead of a number a reader can read at a
        glance."""
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=72555,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED, recent_failed=1,
                            recent_runs=311, recent_checked_at=int(time.time()))
        page = self.page('/escapes')
        self.assertIn('0.3%', page)
        self.assertNotIn('0.321543%', page)

    def test_a_stored_escape_with_no_recent_runs_shows_a_dash_and_no_none(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=72555,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED, recent_runs=None,
                            recent_failed=None, recent_checked_at=None)
        page = self.page('/escapes')
        self.assertNotIn('None', page)
        self.assertIn(formatting.MISSING, page)

    def test_the_landing_time_shown_is_the_one_stored_on_the_row(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=72555,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        self.assertIn(formatting.moment(fixtures.DEFAULT_BUILD_TIME), self.page('/escapes'))

    def test_an_escape_with_no_stored_landing_time_says_so_rather_than_printing_a_date(self) -> None:
        """The backfill leaves a row whose landing the database no longer holds at null. The counts
        either side of that landing are still the evidence, so the row is listed and the time it
        cannot name is named as unknown."""
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=72555,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED, landed_at=None)
        page = self.page('/escapes')
        self.assertIn('Landing time unknown', page)
        self.assertIn('fast/a.html', page)
        self.assertIn('3 of 3', page)

    def test_the_rate_is_over_what_main_answered_and_not_over_every_conviction(self) -> None:
        """A conviction main ran nothing about belongs in no denominator: counting it would report
        an escape rate that falls as coverage falls."""
        first = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                 pr_title='One')
        second = self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, pr_id=2,
                                  pr_title='Two')
        third = self.store_build(3, flaky={'fast/c.html': config.CLEAN_TREE}, pr_id=3,
                                 pr_title='Three')
        self._stored_escape(first, 'fast/a.html', escapes.ESCAPED)
        self._stored_escape(second, 'fast/b.html', escapes.CONTAINED, failed_after=0)
        self._stored_escape(third, 'fast/c.html', escapes.NO_RUNS, failed_after=0, runs_after=0)
        page = self.page('/escapes')
        self.assertIn('<span class="value">50%</span>', page)
        self.assertIn('1 of 2 convictions main answered', page)

    def test_serving_the_page_never_reaches_results_webkit_org(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        with mock.patch('ews_dashboard.results.urllib.request.urlopen',
                        side_effect=AssertionError('a page made an HTTP request')):
            self.assertEqual(self.client.get('/escapes').status_code, 200)

    def test_a_queue_that_convicted_nothing_escaping_shows_the_page_narrowed(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, builder=fixtures.GTK_BUILDER,
                         builder_id=9, pr_id=2, pr_title='Two')
        page = self.page(f'/escapes?builder={fixtures.GTK_BUILDER}')
        self.assertNotIn('fast/a.html', page)
        self.assertIn('No conviction in this window escaped', page)

    def test_a_verdict_drilled_into_lists_its_convictions_and_why(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=72555,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.CONTAINED, failed_after=0,
                            runs_after=5)
        page = self.page(f'/escapes?verdict={escapes.CONTAINED}')
        self.assertIn('Main ran it <strong>5 times</strong> after the landing and never failed it.',
                      page)
        self.assertIn('pull/72555', page)

    def test_a_verdict_that_is_not_a_verdict_is_ignored_rather_than_refused(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.CONTAINED, failed_after=0)
        page = self.page('/escapes?verdict=NONSENSE')
        self.assertNotIn('never failed it', page)
        self.assertEqual(self.selected_category(page), escapes.ESCAPED)

    def test_the_verdict_being_drilled_into_survives_a_change_of_queue_or_window(self) -> None:
        """The drill-down is a query parameter, so a link that dropped it would close the card the
        moment a reader narrowed the page."""
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.CONTAINED, failed_after=0)
        page = self.page(f'/escapes?verdict={escapes.CONTAINED}')
        self.assertRegex(page, rf'class="badge active" href="[^"]*verdict={escapes.CONTAINED}')
        self.assertRegex(page, rf'class="entry[^"]*"[^>]*href="[^"]*verdict={escapes.CONTAINED}')

    def test_the_page_shows_the_escapes_it_exists_for_when_nothing_is_chosen(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        page = self.page('/escapes')
        self.assertEqual(self.selected_category(page), escapes.ESCAPED)
        self.assertIn('fast/a.html', page)

    def test_a_group_that_convicted_nothing_escaping_shows_the_page_narrowed(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, builder=fixtures.GTK_BUILDER,
                         builder_id=9, pr_id=2, pr_title='Two')
        page = self.page('/escapes?group=GTK')
        self.assertNotIn('fast/a.html', page)
        self.assertIn('No conviction in this window escaped', page)

    def test_a_chosen_category_is_the_selected_one_and_the_default_is_not(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.NO_RUNS, failed_after=0, runs_after=0)
        self.assertEqual(self.selected_category(self.page(f'/escapes?verdict={escapes.NO_RUNS}')),
                         escapes.NO_RUNS)

    def test_every_verdict_is_a_category_even_the_ones_nothing_reached(self) -> None:
        """A bucket nothing landed in has to read as a zero rather than vanish, or a reader cannot
        tell an empty bucket from one this page never checks."""
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.CONTAINED, failed_after=0)
        page = self.page('/escapes')
        for verdict in escapes.VERDICTS:
            expected = 1 if verdict == escapes.CONTAINED else 0
            self.assertRegex(page, rf'state-{verdict}">{verdict}</span></span>\s*'
                                   rf'<span class="tally">{expected}</span>')
            self.assertIn(f'verdict={verdict}', page)

    def test_a_category_previews_its_meaning_and_the_chosen_one_reads_it_out(self) -> None:
        """Six descriptions under six pills made the pane taller than the escapes beside it, so an
        unselected category previews its meaning on hover and the selected one says it in the detail
        pane, where there is room for a line of prose."""
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.CONTAINED, failed_after=0)
        page = self.page(f'/escapes?verdict={escapes.CONTAINED}')
        meaning = escapes.VERDICT_DESCRIPTIONS[escapes.CONTAINED]
        self.assertIn(f'title="{meaning}"', page)
        self.assertIn(f'<p class="meaning text tiny">{meaning}</p>', page)

    def test_the_meaning_gives_way_to_the_placeholder_when_there_is_nothing_to_list(self) -> None:
        """Two paragraphs saying overlapping things read as a pane arguing with itself, and the
        placeholder is the one that explains an empty pane."""
        page = self.page('/escapes')
        self.assertIn('No conviction in this window escaped', page)
        self.assertNotIn('<p class="meaning text tiny">', page)

    def test_the_categories_pane_tallies_the_convictions_it_divides_up(self) -> None:
        first = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                 pr_title='One')
        second = self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, pr_id=2,
                                  pr_title='Two')
        self._stored_escape(first, 'fast/a.html', escapes.CONTAINED, failed_after=0)
        self._stored_escape(second, 'fast/b.html', escapes.NO_RUNS, failed_after=0, runs_after=0)
        page = self.page('/escapes')
        self.assertRegex(page, r'What main said</span>\s*'
                               r'<span class="tally">2 convictions</span>')

    def test_the_verdict_travels_with_the_queue_rail_so_a_queue_holds_the_category(self) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1, pr_title='One')
        page = self.page(f'/escapes?verdict={escapes.CONTAINED}')
        tree_form = re.search(r'<details class="table-filter queue-tree".*?</form>\s*</details>', page, re.S)
        self.assertIsNotNone(tree_form, 'no queue picker form on the page')
        self.assertIn(f'<input type="hidden" name="verdict" value="{escapes.CONTAINED}">',
                      tree_form.group(0))
        narrowed = self.page(f'/escapes?verdict={escapes.CONTAINED}&builder={fixtures.LAYOUT_BUILDER}')
        self.assertEqual(self.selected_category(narrowed), escapes.CONTAINED)

    def test_the_queue_picker_form_holds_no_hidden_copy_of_the_queue_selection(self) -> None:
        """The picker's own checkboxes are the queue selection, so its form cannot also carry a
        hidden `group`/`version`/`builder`: a hidden copy is what let an unticked checkbox keep
        submitting the old value, since the browser sends nothing for an unticked box but a hidden
        input beside it still would."""
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, builder=fixtures.GTK_BUILDER,
                         builder_id=9)
        page = self.page('/escapes?group=GTK')
        tree_form = re.search(r'<details class="table-filter queue-tree".*?</form>\s*</details>', page, re.S)
        self.assertIsNotNone(tree_form, 'no queue picker form on the page')
        form = tree_form.group(0)
        self.assertNotRegex(form, r'<input type="hidden" name="group"')
        self.assertNotRegex(form, r'<input type="hidden" name="version"')
        self.assertNotRegex(form, r'<input type="hidden" name="builder"')

    def test_the_headline_counts_a_low_rate_escape_as_an_escape(self) -> None:
        """A conviction that excused something main had not been failing escaped whether the
        failures after were many or few, so the figure the page leads with holds both."""
        first = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                 pr_title='One')
        second = self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, pr_id=2,
                                  pr_title='Two')
        third = self.store_build(3, flaky={'fast/c.html': config.CLEAN_TREE}, pr_id=3,
                                 pr_title='Three')
        self._stored_escape(first, 'fast/a.html', escapes.ESCAPED)
        self._stored_escape(second, 'fast/b.html', escapes.ESCAPED, failed_after=1, runs_after=96)
        self._stored_escape(third, 'fast/c.html', escapes.CONTAINED, failed_after=0)
        page = self.page('/escapes')
        self.assertIn('2 of 3 convictions main answered', page)

    def test_a_low_rate_escape_is_no_category_of_its_own(self) -> None:
        """One bucket, split under itself: a sibling category would let a reader add the two and
        pass the escape count."""
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED, failed_after=1,
                            runs_after=96)
        page = self.page('/escapes')
        self.assertRegex(page, rf'state-{escapes.ESCAPED}">{escapes.ESCAPED}</span>'
                               r'</span>\s*<span class="tally">1</span>')
        self.assertNotIn('ESCAPED_RARELY', page)

    def test_the_placeholder_does_not_claim_nothing_escaped_on_few_failures(self) -> None:
        """The headline counts it, so a pane saying nothing escaped would argue with the number
        above it."""
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED, failed_after=1,
                            runs_after=96)
        page = self.page('/escapes')
        self.assertNotIn('No conviction in this window escaped:', page)
        self.assertIn('so the escape rests on few failures', page)

    def test_the_rate_split_under_the_escape_bucket_adds_up_to_it(self) -> None:
        strong = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                  pr_title='One')
        rare = self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, pr_id=2,
                                pr_title='Two')
        self._stored_escape(strong, 'fast/a.html', escapes.ESCAPED)
        self._stored_escape(rare, 'fast/b.html', escapes.ESCAPED, failed_after=1, runs_after=96)
        page = self.page('/escapes')
        self.assertRegex(page, rf'{config.ESCAPE_FAILURE_PCT}% of the runs or more</span>'
                               r'<span class="tally">1</span>')
        self.assertRegex(page, r'fewer than that</span><span class="tally">1</span>')

    def currency_counts(self, page: str) -> dict:
        """Every line of the currency split, by its label, so a test can add them up."""
        return {label: int(count) for label, count in re.findall(
            r'<span class="label">((?:still|has|not)[^<]*)</span><span class="tally">(\d+)</span>',
            page,
        )}

    def test_the_currency_split_names_both_absences_of_evidence_as_themselves(self) -> None:
        """Four states that partition the escapes: main not having run the test is no more a recovery
        than nobody having asked is, and the recovered count is the one read as reassurance."""
        still = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                 pr_title='One')
        recovered = self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, pr_id=2,
                                     pr_title='Two')
        not_run = self.store_build(3, flaky={'fast/c.html': config.CLEAN_TREE}, pr_id=3,
                                   pr_title='Three')
        unchecked = self.store_build(4, flaky={'fast/d.html': config.CLEAN_TREE}, pr_id=4,
                                     pr_title='Four')
        now = int(time.time())
        self._stored_escape(still, 'fast/a.html', escapes.ESCAPED, recent_runs=40,
                            recent_failed=31, recent_checked_at=now)
        self._stored_escape(recovered, 'fast/b.html', escapes.ESCAPED, recent_runs=44,
                            recent_failed=0, recent_checked_at=now)
        self._stored_escape(not_run, 'fast/c.html', escapes.ESCAPED, recent_runs=0,
                            recent_failed=0, recent_checked_at=now)
        self._stored_escape(unchecked, 'fast/d.html', escapes.ESCAPED)
        page = self.page('/escapes')

        counts = self.currency_counts(page)
        self.assertEqual(counts, {'still failing it': 1, 'has stopped failing it': 1,
                                  'has not run it lately': 1, 'not checked yet': 1})
        self.assertRegex(page, rf'state-{escapes.ESCAPED}">{escapes.ESCAPED}</span></span>\s*'
                               r'<span class="tally">4</span>')
        self.assertEqual(sum(counts.values()), 4)

    def test_a_listed_escape_says_what_main_is_doing_with_it_now(self) -> None:
        still = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                 pr_title='One')
        recovered = self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE}, pr_id=2,
                                     pr_title='Two')
        now = int(time.time())
        self._stored_escape(still, 'fast/a.html', escapes.ESCAPED, recent_runs=40,
                            recent_failed=31, recent_checked_at=now)
        self._stored_escape(recovered, 'fast/b.html', escapes.ESCAPED, recent_runs=44,
                            recent_failed=0, recent_checked_at=now)
        page = self.page('/escapes')
        self.assertIn(f'Main is still failing it, <strong>31 of 40</strong> runs in the last '
                      f'{config.CURRENCY_DAYS} days.', page)
        self.assertIn(f'Main has stopped failing it: none of its <strong>44 runs</strong> in the '
                      f'last {config.CURRENCY_DAYS} days did.', page)

    def test_a_listed_escape_main_has_not_run_lately_says_the_answer_is_unmeasured(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED, recent_runs=0,
                            recent_failed=0, recent_checked_at=int(time.time()))
        page = self.page('/escapes')
        self.assertIn(f'Main has not run it in the last {config.CURRENCY_DAYS} days, so whether the '
                      'failure is still there is unmeasured.', page)
        self.assertNotIn('Main has stopped failing it', page)

    def test_an_unchecked_escape_claims_nothing_about_the_last_week(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        page = self.page('/escapes')
        self.assertIn('Main failed it <strong>3 of 3</strong>', page)
        self.assertNotIn('Main is still failing it', page)
        self.assertNotIn('Main has stopped failing it', page)
        self.assertNotIn('Main has not run it', page)

    def test_no_split_is_shown_where_nothing_escaped(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.CONTAINED, failed_after=0)
        self.assertNotIn('<div class="subcategories">', self.page('/escapes'))

    def test_the_counts_behind_a_verdict_are_emphasised_where_a_reader_looks_for_them(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.FAILS_ON_MAIN, failed_after=7,
                            runs_after=58)
        page = self.page(f'/escapes?verdict={escapes.FAILS_ON_MAIN}')
        self.assertIn('<strong>7 of 58</strong>', page)
        self.assertIn('main&#39;s failure, not this change&#39;s.', page)

    def test_the_detail_pane_narrows_to_the_chosen_queue(self) -> None:
        """The counts beside it are narrowed, so a list that was not would name a test the queue
        rail says is not there."""
        first = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                 pr_title='One')
        second = self.store_build(2, flaky={'fast/b.html': config.CLEAN_TREE},
                                  builder=fixtures.GTK_BUILDER, builder_id=9, pr_id=2,
                                  pr_title='Two')
        self._stored_escape(first, 'fast/a.html', escapes.CONTAINED, failed_after=0)
        self._stored_escape(second, 'fast/b.html', escapes.CONTAINED, failed_after=0)
        page = self.page(f'/escapes?verdict={escapes.CONTAINED}&builder={fixtures.GTK_BUILDER}')
        self.assertIn('fast/b.html', page)
        self.assertNotIn('fast/a.html', page)

    def test_the_legend_names_the_configured_window_and_currency_lengths(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        page = self.page('/escapes')
        self.assertIn(f'{config.ESCAPE_WINDOW_DAYS} days', page)
        self.assertIn(str(config.CURRENCY_DAYS), page)

    def test_the_legend_cites_the_wilson_score_interval_it_uses(self) -> None:
        build_id = self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE}, pr_id=1,
                                    pr_title='One')
        self._stored_escape(build_id, 'fast/a.html', escapes.ESCAPED)
        page = self.page('/escapes')
        self.assertIn(
            'https://en.wikipedia.org/wiki/Binomial_proportion_confidence_interval'
            '#Wilson_score_interval', page)


class TestVocabularyLegend(WebTest):
    """The vocabulary legend at the foot of `/explore` and `/tests`, glossing the chips a reader
    otherwise has no explanation for."""

    LEGEND = '<details class="section legend-pane" open>'

    def legend(self, page: str) -> str:
        return page[page.index(self.LEGEND):]

    def legend_row(self, legend: str, state_class: str) -> tuple:
        """The gloss and, if the row has one, the count in the legend row for one chip class."""
        found = re.search(
            rf'<span class="state {re.escape(state_class)}">[^<]*</span>\s*(?:</a>\s*)?</td>\s*'
            r'<td class="text tiny">([^<]*)</td>\s*(?:<td class="numeric">([^<]*)</td>)?',
            legend)
        self.assertIsNotNone(found, f'no legend row for {state_class}')
        return found.group(1), found.group(2)

    def legend_link(self, legend: str, state_class: str) -> str:
        found = re.search(
            rf'<a class="legend-entry" href="([^"]*)">\s*<span class="state {re.escape(state_class)}">',
            legend)
        self.assertIsNotNone(found, f'no linked legend entry for {state_class}')
        return found.group(1).replace('&amp;', '&')

    def test_the_pane_needs_no_javascript(self) -> None:
        self.assertIn(self.LEGEND, self.page('/explore'))
        self.assertIn(self.LEGEND, self.page('/tests'))

    def test_every_state_choice_gets_its_own_legend_row(self) -> None:
        legend = self.legend(self.page('/explore'))
        for choice in formatting.STATE_CHOICES:
            gloss, _ = self.legend_row(legend, f'state-{choice}')
            self.assertEqual(gloss, false_positive.BUCKET_DESCRIPTIONS[choice])

    def test_explore_covers_test_verdicts_too(self) -> None:
        legend = self.legend(self.page('/explore'))
        for verdict in formatting.VERDICT_CHOICES:
            gloss, _ = self.legend_row(legend, f'state-{verdict}')
            self.assertEqual(gloss, false_positive.VERDICT_DESCRIPTIONS[verdict])

    def test_explore_does_not_cover_flake_types(self) -> None:
        legend = self.legend(self.page('/explore'))
        for rule in config.FLAKINESS_RULES:
            self.assertNotIn(config.RULE_DESCRIPTIONS[rule], legend)

    def test_tests_covers_flake_types(self) -> None:
        legend = self.legend(self.page('/tests'))
        for rule in config.FLAKINESS_RULES:
            self.assertIn(config.RULE_DESCRIPTIONS[rule], legend)

    def test_tests_does_not_cover_build_states_or_test_verdicts(self) -> None:
        legend = self.legend(self.page('/tests'))
        descriptions = (
            [false_positive.BUCKET_DESCRIPTIONS[choice] for choice in formatting.STATE_CHOICES]
            + [false_positive.VERDICT_DESCRIPTIONS[verdict] for verdict in formatting.VERDICT_CHOICES]
        )
        for description in descriptions:
            self.assertNotIn(description, legend)
        for outer in descriptions:
            for inner in descriptions:
                if outer is not inner:
                    self.assertNotIn(inner, outer,
                                     f'{inner!r} is a substring of {outer!r}, so a future gloss could '
                                     'start passing this test as a false negative')

    def test_a_build_state_chip_links_to_the_pane_narrowed_to_that_state_and_the_link_narrows_it(
        self,
    ) -> None:
        noise = self.store_build(1, first=['fast/pre.html'], second=['fast/pre.html'], clean=[])
        self.store_build(2, first=['fast/real.html'], second=['fast/real.html'], clean=[])
        self.classify_everything({'fast/pre.html': UNRELIABLE, 'fast/real.html': RELIABLE})
        page = self.page(f'/explore?state={formatting.ANY_STATE}')
        legend = self.legend(page)
        href = self.legend_link(legend, f'state-{false_positive.FALSE_RED}')
        self.assertIn(f'state={false_positive.FALSE_RED}', href)
        narrowed = self.page(href)
        builds_start = narrowed.index('<div class="pane builds">')
        builds_end = narrowed.index('<div class="pane detail">', builds_start)
        builds_pane = narrowed[builds_start:builds_end]
        self.assertEqual(re.findall(r'<a class="entry[^"]*" href="[^"]*build=(\d+)', builds_pane),
                         [str(noise)])

    def test_a_flake_type_chip_links_to_the_pane_narrowed_to_that_rule_and_the_link_narrows_it(
        self,
    ) -> None:
        self.store_build(1, flaky={'fast/a.html': config.CLEAN_TREE})
        self.store_build(2, flaky={'fast/b.html': config.DIRTY_TREE})
        legend = self.legend(self.page('/tests'))
        found = re.search(r'<a class="legend-entry" href="([^"]*)">CleanTree</a>', legend)
        self.assertIsNotNone(found, 'no legend entry linking CleanTree')
        href = found.group(1).replace('&amp;', '&')
        self.assertIn('rule:anyof:CleanTree', href)
        narrowed = self.page(href)
        start = narrowed.index('id="convicted"')
        end = narrowed.index(self.LEGEND)
        table = narrowed[start:end]
        self.assertIn('fast/a.html', table)
        self.assertNotIn('fast/b.html', table)

    def test_a_build_state_count_agrees_with_the_metric_above_it(self) -> None:
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self.store_build(2, first=['fast/b.html'], second=['fast/b.html'], clean=[])
        self.classify_everything({'fast/a.html': UNRELIABLE})
        page = self.page('/explore')
        found = re.search(r'<span class="value">(\d+)</span>\s*'
                          r'<span class="text tiny denominator">failing builds awaiting a refresh</span>',
                          page)
        self.assertIsNotNone(found, 'no "Not classified yet" metric on the page')
        metric_count = found.group(1)
        legend = self.legend(page)
        _, legend_count = self.legend_row(legend, f'state-{formatting.UNCLASSIFIED}')
        self.assertEqual(legend_count, metric_count)

    def test_the_false_positive_verdict_glosses_name_the_configured_pass_rate_threshold(self) -> None:
        for verdict in (false_positive.REAL, false_positive.PRE_EXISTING):
            self.assertIn(str(config.PRE_EXISTING_THRESHOLD_PCT),
                          false_positive.VERDICT_DESCRIPTIONS[verdict])

    def test_the_too_many_surfaced_reason_names_the_configured_ceiling(self) -> None:
        self.assertIn(str(config.MAX_CLASSIFIABLE_SURFACED_TESTS),
                      false_positive.REASON_DESCRIPTIONS[false_positive.TOO_MANY_SURFACED])

    def test_the_escape_verdict_glosses_name_the_configured_window_and_failure_share(self) -> None:
        for verdict in (escapes.ESCAPED, escapes.FAILS_ON_MAIN, escapes.CONTAINED, escapes.NO_RUNS,
                        escapes.NO_BASELINE):
            self.assertIn(f'{config.ESCAPE_WINDOW_DAYS} days',
                          escapes.VERDICT_DESCRIPTIONS[verdict])
        self.assertIn(str(config.ESCAPE_FAILURE_PCT), escapes.VERDICT_DESCRIPTIONS[escapes.ESCAPED])


class TestMethodologyDisclosure(WebTest):
    """The methodology prose went unread sitting open at the bottom of a page, so it is collapsed
    behind a details, which the browser opens without the JavaScript this app does not ship."""

    DISCLOSURE = '<details class="caveats-disclosure">'

    def assert_collapsed(self, path: str, prose: str) -> None:
        page = self.page(path)
        self.assertIn(self.DISCLOSURE, page)
        self.assertNotIn('caveats-disclosure" open', page)
        self.assertIn(prose, page)

    def test_the_landing_methodology_is_present_and_collapsed(self) -> None:
        self.assert_collapsed('/', 'every rate here is a floor on blame noise')

    def test_the_escape_methodology_is_present_and_collapsed(self) -> None:
        self.assert_collapsed('/escapes',
                              'An escape is a test main failed after the landing')


class TestReadOnly(WebTest):
    def test_serving_a_page_never_reaches_results_webkit_org(self) -> None:
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        with mock.patch('ews_dashboard.results.urllib.request.urlopen',
                        side_effect=AssertionError('a page made an HTTP request')):
            self.assertEqual(self.client.get('/').status_code, 200)
            self.assertEqual(self.client.get('/explore').status_code, 200)

    def test_serving_a_page_writes_no_classification(self) -> None:
        self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self.page('/')
        self.assertEqual(self.connection.execute(
            'SELECT COUNT(*) FROM build_classifications').fetchone()[0], 0)

    def test_serving_a_selected_build_never_reaches_results_webkit_org(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        with mock.patch('ews_dashboard.results.urllib.request.urlopen',
                        side_effect=AssertionError('a page made an HTTP request')):
            self.assertEqual(self.client.get(f'/explore?build={build_id}').status_code, 200)

    def test_serving_a_selected_build_writes_no_classification(self) -> None:
        build_id = self.store_build(1, first=['fast/a.html'], second=['fast/a.html'], clean=[])
        self.page(f'/explore?build={build_id}')
        self.assertEqual(self.connection.execute(
            'SELECT COUNT(*) FROM build_classifications').fetchone()[0], 0)


class TestFormatting(WebTest):
    def test_an_age_reads_as_an_age(self) -> None:
        self.assertEqual(formatting.age(None), 'never')
        self.assertEqual(formatting.age(30), 'moments ago')
        self.assertEqual(formatting.age(3600), '1 hour ago')
        self.assertEqual(formatting.age(7200), '2 hours ago')
        self.assertEqual(formatting.age(86400 * 3), '3 days ago')

    def test_a_missing_number_is_not_a_zero(self) -> None:
        self.assertEqual(formatting.percent(None), formatting.MISSING)
        self.assertEqual(formatting.count(None), formatting.MISSING)
        self.assertEqual(formatting.moment(None), formatting.MISSING)
        self.assertEqual(formatting.percent(12.5), '12.5%')
        self.assertEqual(formatting.count(1234), '1,234')

    def test_a_moment_is_reported_in_utc(self) -> None:
        at = datetime.datetime(2026, 8, 20, 15, 30, tzinfo=datetime.timezone.utc)
        self.assertEqual(formatting.moment(int(at.timestamp())), '2026-08-20 15:30 UTC')

    def test_a_day_is_the_date_half_of_a_moment(self) -> None:
        at = datetime.datetime(2026, 8, 20, 15, 30, tzinfo=datetime.timezone.utc)
        self.assertEqual(formatting.day(int(at.timestamp())), '2026-08-20')

    def test_a_missing_day_is_the_shared_sentinel(self) -> None:
        self.assertEqual(formatting.day(None), formatting.MISSING)

    def test_a_clock_is_the_time_half_of_a_moment(self) -> None:
        at = datetime.datetime(2026, 8, 20, 15, 30, tzinfo=datetime.timezone.utc)
        self.assertEqual(formatting.clock(int(at.timestamp())), '15:30 UTC')

    def test_a_missing_clock_is_empty_rather_than_a_second_sentinel(self) -> None:
        self.assertEqual(formatting.clock(None), '')
