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

"""The two pages, both strictly read-only.

A request never reaches the network and never classifies a build; it reads what scripts/refresh.py
left behind. That is structural rather than a convention: the routes are handed
false_positive.cached_classifier, which has no History to ask and reports an unclassified build as
unclassified. Anything the refresh has not caught up with therefore shows as a gap on the page
instead of as a slow request.
"""

from __future__ import annotations

import sqlite3
import time
from dataclasses import dataclass
from typing import Optional, Union

from flask import Flask, Response, g, redirect, render_template, request, url_for
from werkzeug.datastructures import MultiDict

from ews_dashboard import config, db, queues, results, suites
from ews_dashboard.analysis import convictions, escapes, false_positive, filters, freshness, trend
from ews_dashboard.web import chart, formatting, links

DEFAULT_WINDOW_DAYS = 7
WINDOW_CHOICES = (7, 14, 30, 60, 90)
BUILDS_SHOWN = 200

# The freshness banner is dismissed by a request rather than in the browser, because this page needs
# no JavaScript and a dismissal has to hold on the next page too. The cookie holds the signature of
# what was dismissed, so a banner saying something new is shown again.
FRESHNESS_COOKIE = 'freshness_dismissed'
FRESHNESS_DISMISSAL_SECONDS = 30 * 86400

# How many days each point of the trend line averages over. The window drives the chart's span, so
# this is the only thing left to choose: a short average shows every spike, a long one shows whether
# the level moved.
ROLLING_CHOICES = (3, 7, 14, 28)

SUITE_CHOICES = tuple(suite.name for suite in suites.SUITES)

# The column a page of convicted tests reads in when a request asked for no order of its own.
DEFAULT_SORT = 'convictions'

# The submit buttons that grow the filter and sort chip rows by one blank chip apiece. Named rather
# than a shared name with different values, so a request can carry at most one of them and the route
# never has to guess which row a bare "yes" was about.
ADD_FILTER_ARGUMENT = 'add_filter'
ADD_SORT_ARGUMENT = 'add_sort'

# What a chip's kind renders its value control as, when its column has no fixed vocabulary. Anything
# not listed here is free text, `filters.TEXT` included.
CHIP_INPUT_TYPES = {filters.INTEGER: 'number', filters.TIMESTAMP: 'date'}


class Classified:
    """Shared by both panes: the state a page shows for a build, which is not the same as its bucket.

    A build that showed its author no failures has no bucket and is nonetheless fully classified, so
    reporting a missing bucket as unclassified would send a reader to the refresh over a build there
    is nothing to refresh.
    """

    classification: Optional[false_positive.Classification]

    @property
    def state(self) -> Optional[str]:
        if self.classification is None:
            return None
        return self.classification.bucket or formatting.NO_SURFACED


@dataclass(frozen=True)
class BuildSummary(Classified):
    """One failing build as a row in the builds pane.

    Holds the row rather than copying it because the detail pane needs every column the classifier
    reads, and a page that listed a build must be able to open it without a second query.
    """

    build: sqlite3.Row
    classification: Optional[false_positive.Classification]

    @property
    def surfaced_total(self) -> Optional[int]:
        return self.classification.surfaced_total if self.classification else None


@dataclass(frozen=True)
class BuildFilter:
    """What the builds pane is narrowed to inside the window and the scope.

    An unknown state name and an unreadable bound are dropped rather than refused, because both
    arrive from a hand-edited URL as readily as from the form; a minimum above the maximum is left
    alone, so it narrows to nothing and says so.

    `defaulted` marks a `states` that came from `formatting.DEFAULT_STATES` rather than from the
    request, whether because no `state` argument was given or because none of the given ones were
    recognized; the pane reads it to stay closed on the quiet, default view.
    """

    states: tuple
    min_shown: Optional[int]
    max_shown: Optional[int]
    defaulted: bool

    @property
    def narrowing(self) -> bool:
        return bool(self.states) or self.min_shown is not None or self.max_shown is not None

    def matches(self, summary: BuildSummary) -> bool:
        if self.states and (summary.state or formatting.UNCLASSIFIED) not in self.states:
            return False
        shown = summary.surfaced_total or 0
        if self.min_shown is not None and shown < self.min_shown:
            return False
        if self.max_shown is not None and shown > self.max_shown:
            return False
        return True


@dataclass(frozen=True)
class BuildDetail(Classified):
    """One build's own pane: what it showed its author, and why each test landed where it did."""

    build: sqlite3.Row
    configuration: results.Configuration
    classification: Optional[false_positive.Classification]
    undetermined_reason: Optional[str]
    tests: list
    matching: list


@dataclass(frozen=True)
class Window:
    """The span a page counts over: the last `days` days, ending now.

    Rolling rather than anchored to UTC midnight, which made the shortest window mean "today so
    far" — an hour of EWS at 01:00 UTC, and a page that read as having no data at all. The trend
    chart is unaffected, since it buckets by UTC day over TREND_DAYS and never consults a Window.
    """

    days: int
    since: int
    until: int

    @classmethod
    def of_days(cls, days: int) -> 'Window':
        until = int(time.time())
        return cls(days=days, since=until - days * 86400, until=until)


@dataclass(frozen=True)
class FilterChip:
    """One row of the filter surface: the column, operator and value a request committed at this
    index, with what its column allows already resolved, so the template does no registry lookup of
    its own.

    A chip with no column yet — a blank one a reader has not filled in — offers every operator the
    registry knows rather than one column's own, since no column means no kind to look one set of
    operators up under; `condition` still drops whichever of those does not belong to the column a
    reader eventually picks.
    """

    index: int
    column: Optional[str]
    operator: Optional[str]
    value: str
    values: tuple
    many_values: bool
    operators: tuple
    vocabulary: Optional[tuple]
    input_type: str


def _filter_chip(index: int, condition: Optional[filters.Condition],
                 clause: Optional[str] = None) -> FilterChip:
    """One committed condition as a chip, or a blank chip where there is none.

    The value comes from `clause` — the reader's own text — rather than `condition.values`, which
    for `has`/`starts`/`ends` holds the LIKE pattern `prepare` wrapped it in and would show a chip's
    value box a pattern nobody typed.

    `many_values` is the committed operator's own arity, not the column's kind: a vocabulary column
    like `suite` takes both a one-value operator (`eq`) and a many-value one (`in`), and only the
    operator a reader actually chose says which control the value belongs in. `values` is `value`
    split back into the elements a many-value operator's own vocabulary select preselects.
    """
    if condition is None:
        return FilterChip(index=index, column=None, operator=None, value='', values=(),
                          many_values=False, operators=filters.ALL_OPERATORS, vocabulary=None,
                          input_type='text')
    column = condition.column
    value = filters.filter_clause_value(clause) if clause is not None else ''
    many_values = condition.operator.arity == filters.MANY_VALUES
    values = tuple(value.split(filters.LIST_SEPARATOR)) if many_values else ()
    return FilterChip(index=index, column=column.name, operator=condition.operator.name,
                      value=value, values=values, many_values=many_values,
                      operators=tuple(column.operators.values()),
                      vocabulary=column.vocabulary,
                      input_type=CHIP_INPUT_TYPES.get(column.kind, 'text'))


def _filter_chips(asked: filters.Requested, extra: int) -> tuple:
    """Every filter chip the surface shows: one per committed condition, then `extra` blank ones."""
    chips = [_filter_chip(index, condition, clause) for index, (condition, clause)
             in enumerate(zip(asked.conditions, asked.filter_clauses))]
    for _ in range(extra):
        chips.append(_filter_chip(len(chips), None))
    return tuple(chips)


@dataclass(frozen=True)
class SortChip:
    """One row of the sort surface: the column and direction a request committed at this index, or
    the blanks of one not yet filled in."""

    index: int
    column: Optional[str]
    direction: Optional[str]


def _sort_chip(index: int, key: Optional[filters.SortKey]) -> SortChip:
    if key is None:
        return SortChip(index=index, column=None, direction=None)
    return SortChip(index=index, column=key.column.name,
                    direction=filters.DESCENDING if key.descending else filters.ASCENDING)


def _sort_chips(asked: filters.Requested, extra: int) -> tuple:
    """Every sort chip the surface shows: one per committed key, then `extra` blank ones."""
    chips = [_sort_chip(index, key) for index, key in enumerate(asked.sort_keys)]
    for _ in range(extra):
        chips.append(_sort_chip(len(chips), None))
    return tuple(chips)


def _canonical_filter_arguments(table: filters.Table) -> object:
    """The canonical `f.`/`s.` arguments a request asked with, whether it spoke that grammar directly
    or exploded it into per-chip controls.

    A chip form's controls are always named the exploded way — `requested` never reads the exploded
    spelling, so a request that used it would otherwise look like one that asked for nothing at all.
    """
    if not filters.has_exploded_arguments(request.args, table):
        return request.args
    canonical = MultiDict()
    for specification in filters.exploded_filter_specifications(request.args, table):
        canonical.add(filters.filter_argument(table), specification)
    for specification in filters.exploded_sort_specifications(request.args, table):
        canonical.add(filters.sort_argument(table), specification)
    return canonical


def _carried_arguments() -> dict:
    """Every query argument this request carries, minus any beginning with `_`.

    `url_for` takes `_anchor`, `_method`, `_scheme` and `_external` as keyword-only arguments of its
    own, so a reader-named argument spelled the same way would bind to one of those instead of
    becoming part of the URL, and — since every value here is a list from `to_dict(flat=False)` —
    raise rather than render for the three of those that take a single value. No page's own grammar
    starts with `_`, so dropping the whole family here cannot lose an argument a reader meant.
    """
    return {name: values for name, values in request.args.to_dict(flat=False).items()
            if not name.startswith('_')}


def _redirect_target(table: filters.Table) -> Optional[str]:
    """Where a chip-form submission belongs once its exploded columns, operators and values have been
    turned back into the canonical `f.<table>=`/`s.<table>=` spelling this page's own links speak, or
    None where the request already speaks that spelling and needs no redirect.

    Skipped for a `+filter`/`+sort` press: that button asks the page already open for one more blank
    chip, not a new address — the chip it adds has nothing to redirect to yet.
    """
    if ADD_FILTER_ARGUMENT in request.args or ADD_SORT_ARGUMENT in request.args:
        return None
    if not filters.has_exploded_arguments(request.args, table):
        return None
    filter_stem = f'{filters.filter_argument(table)}{filters.CLAUSE_SEPARATOR}'
    sort_stem = f'{filters.sort_argument(table)}{filters.CLAUSE_SEPARATOR}'
    kept = {name: values for name, values in _carried_arguments().items()
            if not name.startswith(filter_stem) and not name.startswith(sort_stem)}
    kept[filters.filter_argument(table)] = list(
        filters.exploded_filter_specifications(request.args, table))
    kept[filters.sort_argument(table)] = list(
        filters.exploded_sort_specifications(request.args, table))
    return url_for('tests', **kept)


def _chosen(name: str, choices: tuple, default: Optional[str] = None) -> Optional[str]:
    """A query argument restricted to a known set, so a hand-edited URL selects nothing unknown."""
    value = request.args.get(name)
    return value if value in choices else default


def _chosen_number(name: str, choices: tuple, default: int) -> int:
    try:
        value = int(request.args.get(name, default))
    except ValueError:
        return default
    return value if value in choices else default


def _bound(name: str) -> Optional[int]:
    """One end of a range, absent when the field was left empty or holds something that is not a
    number."""
    try:
        return int(request.args[name])
    except (KeyError, ValueError):
        return None


def _selection(known_builders: tuple) -> queues.Selection:
    """The reader's queue selection, read from `group`, `version` and `builder`, each repeatable and
    unioned. `version` is `group:version`, group-qualified so a bare version number is never taken
    from the wrong group's vocabulary — `version=iOS:26` must not also select
    `visionOS-26-Simulator-WK2-Tests-EWS`. An unknown group, a `version` that does not split into
    exactly two non-empty parts, and a builder not in this window's own set are all dropped rather
    than refused, matching every other filter here.
    """
    groups = tuple(name for name in request.args.getlist('group')
                   if name in queues.QUEUE_GROUP_NAMES)
    versions = []
    for raw in request.args.getlist('version'):
        parts = raw.split(':', 1)
        if len(parts) != 2:
            continue
        group, version = parts
        if group in queues.QUEUE_GROUP_NAMES and version:
            versions.append((group, version))
    builders = tuple(name for name in request.args.getlist('builder') if name in known_builders)
    return queues.Selection(groups=groups, versions=tuple(versions), builders=builders)


def _queue_summary(selection: queues.Selection, resolved: tuple) -> str:
    """What the collapsed dropdown reads before a reader opens it."""
    if selection.empty:
        return 'All queues'
    count = len(resolved)
    return f'{count} queue' + ('' if count == 1 else 's')


def _build_filter() -> BuildFilter:
    """The builds pane's own filter, read from `state`, `min_shown` and `max_shown`.

    No `state` argument at all is the quiet default: the two noise states rather than every build.
    `formatting.ANY_STATE` asks past that default explicitly, narrowing by state not at all. A list
    that named states but recognized none of them falls back to the same default rather than to
    everything, so a hand-mangled URL cannot silently widen the pane past what a fresh visit shows.
    """
    requested = request.args.getlist('state')
    if not requested:
        states, defaulted = formatting.DEFAULT_STATES, True
    elif formatting.ANY_STATE in requested:
        states, defaulted = (), False
    else:
        recognized = tuple(value for value in requested if value in formatting.STATE_CHOICES)
        states, defaulted = (recognized, False) if recognized else (formatting.DEFAULT_STATES, True)
    return BuildFilter(
        states=states,
        defaulted=defaulted,
        min_shown=_bound('min_shown'),
        max_shown=_bound('max_shown'),
    )


def _window() -> Window:
    return Window.of_days(_chosen_number('days', WINDOW_CHOICES, DEFAULT_WINDOW_DAYS))


def create_app(database_path: Optional[str] = None) -> Flask:
    app = Flask(__name__)
    resolved_path = database_path or config.database_path()
    formatting.register(app)

    def open_database() -> sqlite3.Connection:
        if 'connection' not in g:
            g.connection = db.connect(resolved_path)
        return g.connection

    @app.teardown_appcontext
    def close_connection(exception: Optional[BaseException]) -> None:
        open_connection = g.pop('connection', None)
        if open_connection is not None:
            open_connection.close()

    @app.route('/')
    def landing() -> str:
        return render_template('landing.html', **_landing_context(open_database(), _window()))

    @app.route('/explore')
    def explore() -> str:
        return render_template('explore.html', **_explore_context(open_database(), _window()))

    @app.route('/tests')
    def tests() -> Union[str, Response]:
        target = _redirect_target(filters.TESTS)
        if target is not None:
            return redirect(target)
        return render_template('tests.html', **_tests_context(open_database(), _window()))

    @app.route('/escapes')
    def escaped_regressions() -> str:
        return render_template('escapes.html', **_escapes_context(open_database(), _window()))

    @app.route('/dismiss-freshness')
    def dismiss_freshness() -> Response:
        response = redirect(_internal_target(request.args.get('next')))
        response.set_cookie(FRESHNESS_COOKIE, request.args.get('state', ''),
                            max_age=FRESHNESS_DISMISSAL_SECONDS, samesite='Lax')
        return response

    return app


def _internal_target(target: Optional[str]) -> str:
    """Where a dismissal returns to. The target arrives in a query parameter, so anything that is not
    a path on this app sends the reader back to the overview rather than off the site."""
    if not target or not target.startswith('/') or target.startswith('//') or '\\' in target:
        return url_for('landing')
    return target


def _freshness_context(open_connection: sqlite3.Connection) -> dict:
    current = freshness.current(open_connection)
    return {
        'freshness': current,
        'freshness_dismissed': request.cookies.get(FRESHNESS_COOKIE) == current.signature,
    }


@dataclass(frozen=True)
class Scope:
    """What every page narrows by, and the queue tree a reader picks from.

    `activity` is computed from `suite` alone, never from the queue selection: it is what the
    dropdown's tree is built from, so narrowing to one queue never makes the others disappear from
    the list a reader could still pick. `builders` is the selection resolved to concrete builder
    names against that same list, which is what every analysis call filters by; an empty `builders`
    means no selection was made, which every analysis function reads as no filter at all.
    """

    suite: Optional[str]
    selection: queues.Selection
    builders: tuple
    activity: list
    tree: tuple
    summary: str


def _scope(open_connection: sqlite3.Connection, window: Window) -> Scope:
    suite = _chosen('suite', SUITE_CHOICES)
    activity = convictions.queue_activity(open_connection, window.since, window.until, suite=suite)
    known = tuple(queue.builder for queue in activity)
    selection = _selection(known)
    resolved = queues.resolve(selection, known)
    counts = {queue.builder: queue.convictions for queue in activity}
    return Scope(
        suite=suite,
        selection=selection,
        builders=resolved,
        activity=activity,
        tree=queues.tree(known, counts),
        summary=_queue_summary(selection, resolved),
    )


def _selection_args(selection: queues.Selection) -> dict:
    """`group`/`version`/`builder` as the tuples a template forwards through a link or a hidden
    field, in the URL grammar `_selection` reads back."""
    return {
        'group': selection.groups,
        'version': tuple(f'{group}:{version}' for group, version in selection.versions),
        'builder': selection.builders,
    }


def _landing_context(open_connection: sqlite3.Connection, window: Window) -> dict:
    scope = _scope(open_connection, window)
    rolling = _chosen_number('rolling', ROLLING_CHOICES, trend.ROLLING_DAYS)
    classifier = false_positive.cached_classifier(open_connection)
    points = trend.daily(open_connection, classifier, trend.today(), days=window.days,
                         rolling=rolling, suite=scope.suite, builders=scope.builders)
    return dict(
        window=window,
        window_choices=WINDOW_CHOICES,
        suite=scope.suite,
        suite_choices=SUITE_CHOICES,
        **_selection_args(scope.selection),
        queue_tree=scope.tree,
        queue_summary=scope.summary,
        rolling=rolling,
        rolling_choices=ROLLING_CHOICES,
        counts=false_positive.rate(open_connection, classifier, window.since, window.until,
                                   suite=scope.suite, builders=scope.builders),
        by_rule=convictions.by_rule(open_connection, window.since, window.until,
                                    suite=scope.suite, builders=scope.builders),
        rule_descriptions=config.RULE_DESCRIPTIONS,
        builds_queried=convictions.builds_queried(open_connection, window.since, window.until,
                                                  suite=scope.suite, builders=scope.builders),
        query_failures=convictions.query_failures(open_connection, window.since, window.until,
                                                  suite=scope.suite, builders=scope.builders),
        chart=chart.of_trend(points, trend.deployments_within(points)),
        threshold_pct=config.PRE_EXISTING_THRESHOLD_PCT,
        filter_argument=filters.filter_argument(filters.TESTS),
        links=links,
        **_freshness_context(open_connection),
    )


def _explore_context(open_connection: sqlite3.Connection, window: Window) -> dict:
    scope = _scope(open_connection, window)
    suite, builders = scope.suite, scope.builders
    test_filter = request.args.get('test') or None
    build_filter = _build_filter()
    classifier = false_positive.cached_classifier(open_connection)
    builds, builds_matched = _filtered_builds(open_connection, window, suite, builders,
                                              classifier, build_filter)
    counts = false_positive.rate(
        open_connection, classifier, window.since, window.until, suite=suite, builders=builders,
    )
    return dict(
        window=window,
        window_choices=WINDOW_CHOICES,
        suite=suite,
        suite_choices=SUITE_CHOICES,
        **_selection_args(scope.selection),
        queue_tree=scope.tree,
        queue_summary=scope.summary,
        builds=builds,
        builds_total=false_positive.failing_build_count(
            open_connection, window.since, window.until, suite=suite, builders=builders,
        ),
        builds_matched=builds_matched,
        build_filter=build_filter,
        state_choices=formatting.STATE_CHOICES,
        state_counts={choice: getattr(counts, choice.lower()) for choice in formatting.STATE_CHOICES},
        detail=_build_detail(open_connection, classifier, test_filter),
        test_filter=test_filter,
        verdict_descriptions=false_positive.VERDICT_DESCRIPTIONS,
        reason_descriptions=false_positive.REASON_DESCRIPTIONS,
        counts=counts,
        bucket_descriptions=false_positive.BUCKET_DESCRIPTIONS,
        verdict_choices=formatting.VERDICT_CHOICES,
        links=links,
        **_freshness_context(open_connection),
    )


def _tests_context(open_connection: sqlite3.Connection, window: Window) -> dict:
    scope = _scope(open_connection, window)
    suite, builders = scope.suite, scope.builders
    test_filter = request.args.get('test') or None
    asked = filters.requested(_canonical_filter_arguments(filters.TESTS), filters.TESTS)
    primary = _primary_sort(asked.sort_keys)
    convicted = convictions.convicted_tests(
        open_connection, window.since, window.until,
        suite=suite, builders=builders,
        conditions=asked.conditions, sort_keys=_test_order(asked.sort_keys),
    )
    drilldown = (
        convictions.test_convictions(open_connection, window.since, window.until, test_filter,
                                     suite=suite, builders=builders)
        if test_filter else None
    )
    return dict(
        window=window,
        window_choices=WINDOW_CHOICES,
        suite=suite,
        suite_choices=SUITE_CHOICES,
        **_selection_args(scope.selection),
        queue_tree=scope.tree,
        queue_summary=scope.summary,
        convicted=convicted,
        rule_descriptions=config.RULE_DESCRIPTIONS,
        flake_types=config.FLAKINESS_RULES,
        rule_counts=convictions.by_rule(open_connection, window.since, window.until,
                                        suite=suite, builders=builders),
        sort=primary.column.name,
        descending=primary.descending,
        descending_first=filters.TESTS.descending_first,
        tests_table=filters.TESTS,
        asked=asked,
        test_filter=test_filter,
        drilldown=drilldown,
        current_args=_carried_arguments(),
        filter_argument=filters.filter_argument(filters.TESTS),
        sort_argument=filters.sort_argument(filters.TESTS),
        filter_chips=_filter_chips(asked, 1 if ADD_FILTER_ARGUMENT in request.args else 0),
        sort_chips=_sort_chips(asked, 1 if ADD_SORT_ARGUMENT in request.args else 0),
        add_filter_argument=ADD_FILTER_ARGUMENT,
        add_sort_argument=ADD_SORT_ARGUMENT,
        filter_chip_argument=filters.filter_chip_argument,
        sort_chip_argument=filters.sort_chip_argument,
        all_operators=filters.ALL_OPERATORS,
        input_types=CHIP_INPUT_TYPES,
        links=links,
        **_freshness_context(open_connection),
    )


def _filtered_builds(
    open_connection: sqlite3.Connection,
    window: Window,
    suite: Optional[str],
    builders: tuple,
    classifier: false_positive.Classifier,
    build_filter: BuildFilter,
) -> tuple:
    """The builds pane's page, and how many builds in the whole window the filter matched.

    A narrowed pane has to narrow before it takes its page: narrowing the newest `BUILDS_SHOWN`
    instead read as "0 of 13,934" over 90 days for a state that only older builds are in. Fetching
    the window unlimited to do it is affordable because the refresh has already classified every one
    of these builds, so the classifier reads its answers back rather than deciding them. An
    unnarrowed pane keeps the cheap page, since matching every build against a filter that matches
    everything would classify thousands of rows to change nothing.
    """
    if not build_filter.narrowing:
        rows = false_positive.failing_builds(
            open_connection, window.since, window.until,
            suite=suite, builders=builders, limit=BUILDS_SHOWN,
        )
        return _build_summaries(rows, classifier, build_filter), None
    rows = false_positive.failing_builds(
        open_connection, window.since, window.until, suite=suite, builders=builders,
    )
    matched = _build_summaries(rows, classifier, build_filter)
    return matched[:BUILDS_SHOWN], len(matched)


def _build_summaries(
    rows: list,
    classifier: false_positive.Classifier,
    build_filter: BuildFilter,
) -> list:
    """The builds pane's rows, narrowed here rather than in the template.

    Not narrowed in SQL: a row's state is the classification's bucket read through `Classified`,
    which turns a missing bucket into `no_surfaced` and a missing classification into unclassified,
    and none of those three are a column.
    """
    return [
        summary for summary in (BuildSummary(row, classifier(row)) for row in rows)
        if build_filter.matches(summary)
    ]


def _escapes_context(open_connection: sqlite3.Connection, window: Window) -> dict:
    """The escape page: what main did with each convicted test after the change landed.

    Read-only like the others. Deciding an escape needs results.webkit.org and a checkout, so this
    page shows what the refresh has already decided and says how much it could not.
    """
    scope = _scope(open_connection, window)
    verdict_shown = _chosen('verdict', escapes.VERDICTS, escapes.ESCAPED)
    drilled = escapes.convictions(open_connection, window.since, window.until, verdict_shown,
                                  suite=scope.suite, builders=scope.builders)
    return dict(
        window=window,
        window_choices=WINDOW_CHOICES,
        suite=scope.suite,
        suite_choices=SUITE_CHOICES,
        **_selection_args(scope.selection),
        queue_tree=scope.tree,
        queue_summary=scope.summary,
        tally=escapes.tally(open_connection, window.since, window.until,
                            suite=scope.suite, builders=scope.builders),
        escaped_verdict=escapes.ESCAPED,
        subcategories=escapes.escape_subcategories(open_connection, window.since, window.until,
                                                   suite=scope.suite, builders=scope.builders),
        convictions=drilled,
        verdict_shown=verdict_shown,
        sentence=escapes.sentence,
        verdict_descriptions=escapes.VERDICT_DESCRIPTIONS,
        verdicts=escapes.VERDICTS,
        window_days=config.ESCAPE_WINDOW_DAYS,
        failure_pct=config.ESCAPE_FAILURE_PCT,
        currency_days=config.CURRENCY_DAYS,
        links=links,
        **_freshness_context(open_connection),
    )


FALLBACK_SORT = ((DEFAULT_SORT, True), ('last_seen', True))


def _primary_sort(keys: tuple) -> filters.SortKey:
    """The key a column heading marks as the one the table is ordered by, which is the first key a
    request asked for or the default where it asked for none."""
    if keys:
        return keys[0]
    return filters.sort_keys(filters.TESTS, ((DEFAULT_SORT, True),))[0]


def _test_order(keys: tuple) -> tuple:
    """The sort keys behind a page of convicted tests: what the request asked for, then the two counts
    every view falls back on before the tiebreak `filters` adds.

    A request that asked for no order at all therefore reads by convictions, so the rows a reader
    opening the page unprompted is looking for arrive first; `order_by` drops a fallback that repeats
    a column already ordered on.
    """
    return tuple(keys) + filters.sort_keys(filters.TESTS, FALLBACK_SORT)


def _build_detail(
    open_connection: sqlite3.Connection,
    classifier: false_positive.Classifier,
    test_filter: Optional[str],
) -> Optional[BuildDetail]:
    """The selected build, looked up by id rather than found in the listed page, so a link to a build
    outside the current window or filter still opens it.

    `tests` stays whole and `matching` carries the filter, so a filter that matches nothing reads as
    a filter that matched nothing rather than as a build that surfaced nothing.

    The tests main already fails come first, since those are the ones the build blamed its author for
    and the reason to open this pane at all. `sorted` is stable, so everything else keeps the order
    `explain` returned it in.
    """
    try:
        build_id = int(request.args['build'])
    except (KeyError, ValueError):
        return None
    row = false_positive.failing_build(open_connection, build_id)
    if row is None:
        return None
    surfaced = sorted(false_positive.explain(open_connection, row),
                      key=lambda test: test.verdict != false_positive.PRE_EXISTING)
    return BuildDetail(
        build=row,
        configuration=results.Configuration.of_build(row),
        classification=classifier(row),
        undetermined_reason=false_positive.undetermined_reason(row),
        tests=surfaced,
        matching=[test for test in surfaced if not test_filter or test_filter in test.name],
    )
