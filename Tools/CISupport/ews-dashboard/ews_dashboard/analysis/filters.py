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

"""The one vocabulary a request may filter and sort a table by.

Every column name, operator and SQL expression here is a literal written in this file. A reader's
string reaches sqlite only as a bound parameter, and only after `parse` has matched its column
against a table's registry and its operator against that column's kind — so a name that is not in
the registry, an operator its kind does not allow, or a value that will not coerce drops the whole
condition rather than reaching the query in any form.

`parse` and `clause` are split so that the half which builds SQL cannot be handed a reader's string
at all: `clause` takes `Condition` objects, which hold a registry `Column` and an `Operator` from
that column's kind, and refuses anything else.

`requested` is the door a request comes in through, and the public grammar is written here rather
than in a route: a page asks this module what a query string asked for and gets back conditions,
sort keys, and the clauses to hand forward in its own links.

`exploded_filter_specifications` and `exploded_sort_specifications` are a second door for a chip form,
which cannot compose one filter value out of three controls (or a sort out of two) without
JavaScript: they turn `f.<table>:<index>:column`/`:op`/`:value` and `s.<table>:<index>:column`/
`:direction` back into the same specs `requested` already validates, so a route still hands a
reader's text to `parse` and nowhere else.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Callable, Iterable, Optional

from ews_dashboard import config, suites

TEXT = 'text'
INTEGER = 'integer'
TIMESTAMP = 'timestamp'
ENUM = 'enum'
ENUM_SET = 'enum_set'

DATE_FORMAT = '%Y-%m-%d'

# How many values an operator binds. A `MANY` operator's placeholder count is the count of values
# that coerced, never a number taken from the request.
NO_VALUES = 'none'
ONE_VALUE = 'one'
MANY_VALUES = 'many'

# What separates the elements of an `in` list in one query argument.
LIST_SEPARATOR = '|'

# The stem of every bind name. Generated here and never taken from a request, so two conditions on
# one column cannot collide and a reader cannot name a parameter.
BIND_PREFIX = 'filter'

# The public query grammar: `f.<table>=<column>:<condition>:<value>` filters and
# `s.<table>=<column>:<asc|desc>` sorts, each repeatable and each read in the order it was written.
FILTER_PREFIX = 'f'
SORT_PREFIX = 's'
CLAUSE_SEPARATOR = ':'

ASCENDING = 'asc'
DESCENDING = 'desc'
DIRECTIONS = {ASCENDING: False, DESCENDING: True}


def _escaped_like(matching: str) -> str:
    """A reader's text with every LIKE wildcard escaped, backslash first: escaping the backslash
    after the wildcards would escape the backslashes the wildcards just introduced."""
    return matching.replace('\\', '\\\\').replace('%', '\\%').replace('_', '\\_')


def _like_fragment(matching: str) -> str:
    """A reader's fragment as a LIKE pattern, with the wildcards they typed matched literally."""
    return f'%{_escaped_like(matching)}%'


def _like_prefix(matching: str) -> str:
    return f'{_escaped_like(matching)}%'


@dataclass(frozen=True)
class Operator:
    """One comparison, as the SQL it emits rather than as a name something else translates.

    `template` is formatted with the column's literal expression, the server's bind names, and (for a
    set operator) `{count}` — the count of values that coerced, computed here from `binds` exactly the
    way `placeholders` is — so the only way a value or a count can appear in the result is as
    something this module derived, never a number a request supplied. An empty template emits no
    clause at all.

    `grouped` is true for an operator whose clause describes the whole group a HAVING sees rather than
    one row a WHERE sees, even where its column is not itself an aggregate.
    """

    name: str
    label: str
    template: str
    arity: str = ONE_VALUE
    prepare: Optional[Callable[[str], str]] = None
    grouped: bool = False

    def sql(self, expression: str, binds: tuple) -> str:
        placeholders = ', '.join(f':{bind}' for bind in binds)
        return self.template.format(expression=expression, placeholders=placeholders,
                                    count=len(binds))


def _operator(name: str, label: str, template: str, arity: str = ONE_VALUE,
              prepare: Optional[Callable[[str], str]] = None, grouped: bool = False) -> Operator:
    return Operator(name=name, label=label, template=template, arity=arity, prepare=prepare,
                    grouped=grouped)


# `ne` is spelled `IS NOT` rather than `<>` because sqlite's `<>` is unknown against NULL, so a row
# whose column is NULL would be dropped by a condition a reader reads as keeping it.
_EQUALITY = (
    _operator('eq', 'is', '{expression} = {placeholders}'),
    _operator('ne', 'is not', '{expression} IS NOT {placeholders}'),
)

_PRESENCE = (
    _operator('set', 'is set', '{expression} IS NOT NULL', arity=NO_VALUES),
    _operator('unset', 'is not set', '{expression} IS NULL', arity=NO_VALUES),
    _operator('off', 'off', '', arity=NO_VALUES),
)

_ORDERED = (
    _operator('gt', 'more than', '{expression} > {placeholders}'),
    _operator('ge', 'at least', '{expression} >= {placeholders}'),
    _operator('lt', 'less than', '{expression} < {placeholders}'),
    _operator('le', 'at most', '{expression} <= {placeholders}'),
)

_MATCHING = (
    _operator('has', 'contains', "{expression} LIKE {placeholders} ESCAPE '\\'",
              prepare=_like_fragment),
    _operator('nohas', 'does not contain', "{expression} NOT LIKE {placeholders} ESCAPE '\\'",
              prepare=_like_fragment),
    _operator('starts', 'starts with', "{expression} LIKE {placeholders} ESCAPE '\\'",
              prepare=_like_prefix),
)

_MEMBERSHIP = (
    _operator('in', 'is any of', '{expression} IN ({placeholders})', arity=MANY_VALUES),
    _operator('notin', 'is none of', '{expression} NOT IN ({placeholders})', arity=MANY_VALUES),
)

# Each of these describes the whole group a column's rows were collapsed into rather than one row of
# it, so every one is `grouped` even though a column like `verdict.rule` is not itself an aggregate.
# There is no `is set`/`is not set` here: the query that groups these rows already constrains the
# column to `IS NOT NULL`, so a presence operator on the group would be a control that does nothing.
_SET = (
    _operator('anyof', 'any of',
              'SUM(CASE WHEN {expression} IN ({placeholders}) THEN 1 ELSE 0 END) > 0',
              arity=MANY_VALUES, grouped=True),
    _operator('allof', 'all of',
              'COUNT(DISTINCT CASE WHEN {expression} IN ({placeholders}) THEN {expression} END) '
              '= {count}', arity=MANY_VALUES, grouped=True),
    _operator('oneof', 'exactly one of',
              'COUNT(DISTINCT CASE WHEN {expression} IN ({placeholders}) THEN {expression} END) = 1',
              arity=MANY_VALUES, grouped=True),
    _operator('exactly', 'exactly',
              'COUNT(DISTINCT CASE WHEN {expression} IN ({placeholders}) THEN {expression} END) '
              '= {count} AND COUNT(DISTINCT {expression}) = {count}',
              arity=MANY_VALUES, grouped=True),
    _operator('noneof', 'none of',
              'SUM(CASE WHEN {expression} IN ({placeholders}) THEN 1 ELSE 0 END) = 0',
              arity=MANY_VALUES, grouped=True),
)


def _by_name(operators: tuple) -> dict:
    return {operator.name: operator for operator in operators}


# `off` alone, not the rest of `_PRESENCE`: a set operator already covers presence, since the query
# that groups these rows constrains the column to `IS NOT NULL` before any of them run.
_OFF = tuple(operator for operator in _PRESENCE if operator.name == 'off')

# Which operators each kind allows. A kind mismatch is a miss in this dict, so asking for `gt` on a
# text column fails exactly the way asking for an operator that does not exist fails.
OPERATORS = {
    TEXT: _by_name(_EQUALITY + _MATCHING + _PRESENCE),
    INTEGER: _by_name(_EQUALITY + _ORDERED + _PRESENCE),
    TIMESTAMP: _by_name(_EQUALITY + _ORDERED + _PRESENCE),
    ENUM: _by_name(_EQUALITY + _MEMBERSHIP + _PRESENCE),
    ENUM_SET: _by_name(_SET + _OFF),
}


def _all_operators() -> tuple:
    seen: dict = {}
    for kind_operators in OPERATORS.values():
        for operator in kind_operators.values():
            seen.setdefault(operator.name, operator)
    return tuple(seen.values())


# Every operator any kind allows, deduplicated by name. A chip with no column chosen yet has no kind
# to look an operator set up under, so it offers this instead; `condition` still drops whichever of
# these does not belong to the column the reader eventually picks.
ALL_OPERATORS = _all_operators()


@dataclass(frozen=True)
class Column:
    """One thing a reader can filter or sort by, and the literal SQL it stands for.

    `expression` is what a WHERE or HAVING compares against. An aggregate's is the aggregate
    re-spelled rather than the output alias it is selected under, because the count query groups
    without selecting any aggregate at all and so has no alias in scope; `order_expression` keeps
    using the alias, which the page's own query does select.
    """

    name: str
    label: str
    expression: str
    kind: str
    aggregate: bool = False
    vocabulary: Optional[tuple] = None
    sortable: bool = True
    filterable: bool = True
    descending_first: bool = True

    @property
    def order_expression(self) -> str:
        return self.name if self.aggregate else self.expression

    @property
    def operators(self) -> dict:
        operators = OPERATORS[self.kind]
        if self.aggregate:
            # An aggregate expression is never NULL over a grouped row, so `set`/`unset` can never
            # narrow anything; drop them the way ENUM_SET drops all but `off` of `_PRESENCE`.
            return {name: operator for name, operator in operators.items()
                    if operator.name not in ('set', 'unset')}
        return operators


@dataclass(frozen=True)
class Table:
    """One queryable set of columns, and the order that makes any page of it total.

    `tiebreak` is mandatory: LIMIT/OFFSET over a partial order drops and duplicates rows across page
    boundaries, since nothing obliges sqlite to break a tie the same way in two queries.
    """

    name: str
    tiebreak: str
    columns: dict = field(default_factory=dict)

    def column(self, name: str) -> Optional[Column]:
        return self.columns.get(name)

    @property
    def sortable_names(self) -> tuple:
        return tuple(name for name, column in self.columns.items() if column.sortable)

    @property
    def descending_first(self) -> dict:
        return {name: self.columns[name].descending_first for name in self.sortable_names}


def _table(name: str, tiebreak: str, columns: tuple) -> Table:
    return Table(name=name, tiebreak=tiebreak,
                 columns={column.name: column for column in columns})


SUITE_NAMES = tuple(suite.name for suite in suites.SUITES)

# `queue` has no fixed vocabulary here because the queues that exist are whatever has builds in the
# window a page is reading, not a constant; an open vocabulary still binds its value.
TESTS = _table(
    'tests',
    'verdict.test_name ASC',
    (
        Column('test', 'Test', 'verdict.test_name', TEXT, descending_first=False),
        Column('rule', 'Flake type', 'verdict.rule', ENUM_SET, vocabulary=config.FLAKINESS_RULES,
               sortable=False, descending_first=False),
        Column('convictions', 'Convictions', 'COUNT(*)', INTEGER, aggregate=True),
        Column('queues', 'Queues', 'COUNT(DISTINCT build.builder)', INTEGER, aggregate=True),
        Column('last_seen', 'Last seen', 'MAX(build.started_at)', TIMESTAMP, aggregate=True),
        Column('queue', 'Queue', 'build.builder', ENUM, sortable=False, descending_first=False),
        Column('suite', 'Suite', 'build.suite', ENUM, vocabulary=SUITE_NAMES, sortable=False,
               descending_first=False),
        Column('pr', 'Pull request', 'build.pr_id', INTEGER, sortable=False),
    ),
)

TABLES = {TESTS.name: TESTS}


@dataclass(frozen=True)
class Condition:
    """One validated filter: a registry column, an operator its kind allows, and the values already
    coerced to what will be bound.

    Constructing one is the only validation `clause` needs, which is why it raises rather than
    dropping: a caller reaching here with an operator of the wrong kind or the wrong count of values
    has skipped `parse`, and that is a bug in this program rather than a hand-edited URL.
    """

    column: Column
    operator: Operator
    values: tuple = ()

    def __post_init__(self) -> None:
        if self.operator is not self.column.operators.get(self.operator.name):
            raise ValueError(f'{self.operator.name} is not an operator on {self.column.name}')
        if self.operator.arity == NO_VALUES and self.values:
            raise ValueError(f'{self.operator.name} takes no value')
        if self.operator.arity == ONE_VALUE and len(self.values) != 1:
            raise ValueError(f'{self.operator.name} takes one value')
        if self.operator.arity == MANY_VALUES and not self.values:
            raise ValueError(f'{self.operator.name} takes at least one value')


@dataclass(frozen=True)
class SortKey:
    column: Column
    descending: bool

    @property
    def sql(self) -> str:
        return f'{self.column.order_expression} {"DESC" if self.descending else "ASC"}'

    @property
    def specification(self) -> str:
        return (f'{self.column.name}{CLAUSE_SEPARATOR}'
                f'{DESCENDING if self.descending else ASCENDING}')


def _coerced(column: Column, value: str) -> Optional[object]:
    """One value as what its column's kind binds, or None where it will not coerce."""
    if column.kind == INTEGER:
        try:
            return int(value)
        except (TypeError, ValueError):
            return None
    if column.kind == TIMESTAMP:
        try:
            return int(value)
        except (TypeError, ValueError):
            pass
        try:
            return int(datetime.strptime(value, DATE_FORMAT)
                       .replace(tzinfo=timezone.utc).timestamp())
        except (TypeError, ValueError):
            return None
    if column.kind in (ENUM, ENUM_SET):
        if column.vocabulary is not None and value not in column.vocabulary:
            return None
        return value
    return value


def _values(column: Column, operator: Operator, value: Optional[str]) -> Optional[tuple]:
    if operator.arity == NO_VALUES:
        return ()
    if value is None:
        return None
    if operator.arity == MANY_VALUES:
        coerced = tuple(each for each in (_coerced(column, element)
                                          for element in value.split(LIST_SEPARATOR))
                        if each is not None)
        # `allof` and `exactly` compare `{count}`, which is the bind count, against a
        # `COUNT(DISTINCT ...)`, so a value written twice would ask for more distinct values than the
        # column can hold and match nothing.
        deduplicated = tuple(dict.fromkeys(coerced))
        return deduplicated or None
    single = _coerced(column, value)
    if single is None:
        return None
    return (operator.prepare(single) if operator.prepare else single,)


def condition(table: Table, name: str, operator_name: str,
              value: Optional[str] = None) -> Optional[Condition]:
    """One filter as a reader asked for it, or None where any part of it was not in the vocabulary.

    Dropped rather than refused, and never replaced by a default: a filter that quietly narrowed by
    something else would be read as the narrowing that was asked for.
    """
    column = table.column(name)
    if column is None or not column.filterable:
        return None
    operator = column.operators.get(operator_name)
    if operator is None:
        return None
    values = _values(column, operator, value)
    if values is None:
        return None
    return Condition(column=column, operator=operator, values=values)


def parse(table: Table, specifications: Iterable[tuple]) -> tuple:
    """Every filter in `specifications` that the table's vocabulary accepts, in the order given.

    A specification is `(column, operator, value)` of strings as they arrived, with the value absent
    or None for an operator that takes none.
    """
    parsed = []
    for specification in specifications:
        name, operator_name = specification[0], specification[1]
        value = specification[2] if len(specification) > 2 else None
        each = condition(table, name, operator_name, value)
        if each is not None:
            parsed.append(each)
    return tuple(parsed)


def clause(conditions: Iterable[Condition]) -> tuple:
    """`(where, having, parameters)` for a set of conditions, each half a bare conjunction or empty.

    An aggregate condition lands in the HAVING half because a WHERE is evaluated before the grouping
    that produces it; a `grouped` operator lands there for a different reason even on a column that is
    not itself an aggregate, such as `verdict.rule` — its clause describes the group a reader's set
    condition asks about (all of, any of, ...) rather than one row of it, so it too can only be
    answered after the grouping runs. The caller decides how to attach each half, since a WHERE here
    is one more conjunct of a query's own filter while a HAVING is the whole of the clause.
    """
    where, having, parameters = [], [], {}
    for index, each in enumerate(conditions):
        if not isinstance(each, Condition):
            raise TypeError(f'{each!r} is not a Condition; call parse first')
        if not each.operator.template:
            continue
        binds = tuple(f'{BIND_PREFIX}{index}_{position}'
                      for position in range(len(each.values)))
        parameters.update(zip(binds, each.values))
        (having if each.column.aggregate or each.operator.grouped else where).append(
            each.operator.sql(each.column.expression, binds))
    return ' AND '.join(where), ' AND '.join(having), parameters


def sort_keys(table: Table, requested: Iterable[tuple]) -> tuple:
    """The sort keys a table accepts, as `(column, descending)` pairs of a name and a bool.

    An unknown or unsortable name drops that key rather than falling back to a column of this
    module's choosing, because a page sorted by something a reader did not ask for still reads as
    sorted by what they did.
    """
    keys = []
    for name, descending in requested:
        column = table.column(name)
        if column is not None and column.sortable:
            keys.append(SortKey(column=column, descending=bool(descending)))
    return tuple(keys)


def order_by(table: Table, keys: Iterable[SortKey]) -> str:
    """The ORDER BY for a page of `table`, always ending in its tiebreak.

    A key repeating a column already ordered on is dropped, but only among the requested keys: the
    tiebreak is appended unconditionally, even where a requested key already orders on the same
    column. That can spell the tiebreak twice, but a repeated ORDER BY term cannot change the
    ordering sqlite already committed to, so the second one costs nothing when it is already the
    primary key.
    """
    ordered, seen = [], set()
    for key in keys:
        if key.column.name in seen:
            continue
        seen.add(key.column.name)
        ordered.append(key.sql)
    ordered.append(table.tiebreak)
    return ', '.join(ordered)


def filter_argument(table: Table) -> str:
    return f'{FILTER_PREFIX}.{table.name}'


def sort_argument(table: Table) -> str:
    return f'{SORT_PREFIX}.{table.name}'


# The fields one chip's controls submit under, exploded because three controls (or two, for a sort)
# cannot compose one query value without JavaScript.
FILTER_CHIP_FIELDS = ('column', 'op', 'value')
SORT_CHIP_FIELDS = ('column', 'direction')


def filter_chip_argument(table: Table, index: int, field: str) -> str:
    """The exploded argument name one filter chip's `field` control submits under."""
    return f'{filter_argument(table)}{CLAUSE_SEPARATOR}{index}{CLAUSE_SEPARATOR}{field}'


def sort_chip_argument(table: Table, index: int, field: str) -> str:
    """The exploded argument name one sort chip's `field` control submits under."""
    return f'{sort_argument(table)}{CLAUSE_SEPARATOR}{index}{CLAUSE_SEPARATOR}{field}'


@dataclass(frozen=True)
class Requested:
    """One table's filters and sorts as a request asked for them, with the clauses they were written
    as, so a page hands the same query string forward instead of composing a grammar of its own.

    A filter clause is kept as the reader's own text and a sort clause is respelled canonically: a
    column heading has to build a sort clause out of a column and a direction anyway, while
    respelling a filter would mean turning a prepared LIKE pattern back into the fragment it came
    from.

    `rejected` is every clause that did not parse. Dropping one rather than refusing the request is
    what `condition` already does with a column that is not in the vocabulary, and a page has to be
    able to say which, since a filter that vanished silently reads as one that matched everything.
    """

    conditions: tuple = ()
    sort_keys: tuple = ()
    filter_clauses: tuple = ()
    sort_clauses: tuple = ()
    rejected: tuple = ()

    @property
    def narrowing(self) -> bool:
        """Whether some committed condition actually emits SQL, rather than merely being present.

        An `off` clause is a committed condition with no template at all — it exists only so a chip
        can be turned back on — so counting `filter_clauses` here would tell a reader `off` alone
        narrowed the page, when the query it ran against had no filter in it whatsoever.
        """
        return any(condition.operator.template for condition in self.conditions)


def _condition_of(table: Table, specification: str) -> Optional[Condition]:
    """One `<column>:<condition>:<value>` clause, or None where it is not one this table accepts.

    Split at most twice, so a value keeps any separator of its own — a test name may hold a colon.
    An empty value is no value, which drops every operator that binds one and leaves the three that
    bind none asking for exactly what they ask for without it.
    """
    parts = specification.split(CLAUSE_SEPARATOR, 2)
    if len(parts) < 2:
        return None
    value = parts[2] if len(parts) > 2 else None
    return condition(table, parts[0].strip(), parts[1].strip(), value or None)


def _sort_key_of(table: Table, specification: str) -> Optional[SortKey]:
    """One `<column>:<asc|desc>` clause. A clause naming no direction reads the way that column
    reads first, so the worst rows arrive first for a count and alphabetically for a name."""
    name, _, direction = specification.partition(CLAUSE_SEPARATOR)
    column = table.column(name.strip())
    if column is None or not column.sortable:
        return None
    if not direction:
        return SortKey(column=column, descending=column.descending_first)
    if direction.strip() not in DIRECTIONS:
        return None
    return SortKey(column=column, descending=DIRECTIONS[direction.strip()])


def filter_clause_value(specification: str) -> str:
    """The value a filter clause was written with, split the same way `_condition_of` does — at most
    twice, so a test name holding a colon of its own stays intact — but returned as the reader typed
    it rather than coerced or `prepare`d, since `has`/`starts` wrap it for a LIKE bind and a chip's
    value box should show back what was typed, not what got sent to sqlite."""
    parts = specification.split(CLAUSE_SEPARATOR, 2)
    return parts[2] if len(parts) > 2 else ''


def requested(arguments: object, table: Table) -> Requested:
    """Every filter and sort one table's query arguments ask for, in the order they were written.

    Repetition is how a request asks for more than one: two `f.<table>` arguments are two conditions
    that both apply, and two `s.<table>` arguments are a primary and a secondary key, so document
    order is what decides which is which. `arguments` is a request's multi-dict, read through
    `getlist` for exactly that reason — a plain mapping would keep one value per name and silently
    lose the rest.

    An argument holding nothing is not a rejected clause: it is the empty field a form always offers
    for the next one, and reporting it would tell a reader they got something wrong by submitting.
    """
    conditions, filter_clauses = [], []
    keys, sort_clauses, rejected = [], [], []
    for specification in arguments.getlist(filter_argument(table)):
        if not specification.strip():
            continue
        each = _condition_of(table, specification)
        if each is None:
            rejected.append(specification)
            continue
        conditions.append(each)
        filter_clauses.append(specification)
    for specification in arguments.getlist(sort_argument(table)):
        if not specification.strip():
            continue
        key = _sort_key_of(table, specification)
        if key is None:
            rejected.append(specification)
            continue
        keys.append(key)
        sort_clauses.append(key.specification)
    return Requested(conditions=tuple(conditions), sort_keys=tuple(keys),
                     filter_clauses=tuple(filter_clauses), sort_clauses=tuple(sort_clauses),
                     rejected=tuple(rejected))


def _exploded_chips(arguments: object, prefix: str) -> dict:
    """Every chip's fields, keyed by the index in its argument name.

    Grouped by that index rather than by the order arguments happened to arrive in, so a chip named
    `f.tests:2:column` still lands on chip 2 even if a request (or a hand-edited form) named its
    fields out of order.

    A field's control can submit more than one value under its own name — a many-value operator's
    value control is a `<select multiple>` — so each field is read with `getlist` and rejoined on
    `LIST_SEPARATOR` rather than `get`, which would keep only the first choice a reader made.
    """
    stem = f'{prefix}{CLAUSE_SEPARATOR}'
    chips: dict = {}
    for key in arguments.keys():
        if not key.startswith(stem):
            continue
        index_text, _, field = key[len(stem):].partition(CLAUSE_SEPARATOR)
        try:
            index = int(index_text)
        except ValueError:
            continue
        chips.setdefault(index, {})[field] = LIST_SEPARATOR.join(arguments.getlist(key))
    return chips


def _chip_specification(chip: dict, fields: Iterable[str]) -> str:
    """One chip's fields joined into a canonical clause, with empty trailing fields left out — an
    operator that takes no value, or a sort naming no direction, then reads exactly as the clause a
    reader would have typed for it."""
    parts = [(chip.get(field) or '').strip() for field in fields]
    while parts and not parts[-1]:
        parts.pop()
    return CLAUSE_SEPARATOR.join(parts)


def has_exploded_arguments(arguments: object, table: Table) -> bool:
    """Whether a request named any of `table`'s chips, rather than speaking the canonical grammar."""
    filter_stem = f'{filter_argument(table)}{CLAUSE_SEPARATOR}'
    sort_stem = f'{sort_argument(table)}{CLAUSE_SEPARATOR}'
    return any(key.startswith(filter_stem) or key.startswith(sort_stem)
               for key in arguments.keys())


def exploded_filter_specifications(arguments: object, table: Table) -> tuple:
    """The canonical `<column>:<op>:<value>` filter specs a chip form submitted, in chip-index order.

    A chip form cannot compose one filter value out of three controls without JavaScript, so it
    submits `f.<table>:<index>:column`, `:op` and `:value` instead of one `f.<table>` clause; this is
    what turns those back into the specs `parse`/`requested` already validate, so a reader's text
    still cannot reach SQL except through that path. A chip whose column is empty or not in the
    table's vocabulary is dropped here rather than left for `parse` to reject, since it is not a
    clause a reader wrote at all — it is the blank a form always offers for the next one.
    """
    chips = _exploded_chips(arguments, filter_argument(table))
    specifications = []
    for index in sorted(chips):
        chip = chips[index]
        if table.column((chip.get('column') or '').strip()) is None:
            continue
        specifications.append(_chip_specification(chip, FILTER_CHIP_FIELDS))
    return tuple(specifications)


def exploded_sort_specifications(arguments: object, table: Table) -> tuple:
    """The canonical `<column>:<asc|desc>` sort specs a chip form submitted, in chip-index order."""
    chips = _exploded_chips(arguments, sort_argument(table))
    specifications = []
    for index in sorted(chips):
        chip = chips[index]
        if table.column((chip.get('column') or '').strip()) is None:
            continue
        specifications.append(_chip_specification(chip, SORT_CHIP_FIELDS))
    return tuple(specifications)
