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

"""Find the WPT tests which make up the Interop project's focus areas.

The Interop project (https://wpt.fyi/interop-2026) groups WPT tests into focus
areas. Each focus area maps to one or more labels, and wpt.fyi's metadata API
maps those labels to the tests which carry them.
"""

import logging
from collections import defaultdict
from collections.abc import Mapping, Sequence, Set
from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone
from typing import Any, Callable, Optional
from urllib.parse import urljoin

import requests

_log = logging.getLogger(__name__)

DEFAULT_WPT_FYI = "https://wpt.fyi/"
DEFAULT_CATEGORY_URL = (
    "https://raw.githubusercontent.com/web-platform-tests/"
    "results-analysis/main/interop-scoring/category-data.json"
)
INTEROP_DATA_URL = "/static/interop-data.json"

# This needs to include product=chrome because of https://github.com/web-platform-tests/wpt.fyi/issues/4324
METADATA_URL = "/api/metadata?includeTestLevel=true&product=chrome"

DEFAULT_TIMEOUT_SECONDS = 120


class InteropDataError(ValueError):
    """Raised when the Interop data doesn't contain what was asked for."""


def fetch_json(url: str, timeout: int = DEFAULT_TIMEOUT_SECONDS) -> Any:
    response = requests.get(url, timeout=timeout)
    response.raise_for_status()
    return response.json()


@dataclass(frozen=True)
class InteropYear:
    start_date: date  # inclusive
    end_date: date  # inclusive

    def __post_init__(self) -> None:
        if self.start_date > self.end_date:
            raise ValueError("Start date cannot be after the end date")

    @property
    def year(self) -> int:
        return self.start_date.year


known_interop_years: Set[InteropYear] = {
    InteropYear(start_date=date(2021, 3, 22), end_date=date(2021, 12, 31)),
    InteropYear(start_date=date(2022, 3, 3), end_date=date(2022, 12, 31)),
    InteropYear(start_date=date(2023, 2, 1), end_date=date(2024, 2, 1)),
    InteropYear(start_date=date(2024, 2, 1), end_date=date(2025, 2, 6)),
    InteropYear(start_date=date(2025, 2, 12), end_date=date(2026, 2, 12)),
    InteropYear(start_date=date(2026, 2, 12), end_date=date(2027, 12, 31)),
}


class LabelledTestFinder:
    def __init__(self, json_fetcher: Optional[Callable[[str], Any]] = None) -> None:
        self._json_fetcher = json_fetcher if json_fetcher is not None else fetch_json
        self._interop_data = None
        self._category_data = None
        self._metadata_data = None

    @property
    def interop_data(self) -> Any:
        if self._interop_data is None:
            url = urljoin(DEFAULT_WPT_FYI, INTEROP_DATA_URL)
            _log.info("Loading Interop data from %s", url)
            self._interop_data = self._json_fetcher(url)
            _log.debug("Loaded Interop data")
        return self._interop_data

    @property
    def category_data(self) -> Any:
        if self._category_data is None:
            url = urljoin(DEFAULT_WPT_FYI, DEFAULT_CATEGORY_URL)
            _log.info("Loading Interop category data from %s", url)
            self._category_data = self._json_fetcher(url)
            _log.debug("Loaded Interop category data")
        return self._category_data

    @property
    def metadata_data(self) -> Any:
        if self._metadata_data is None:
            url = urljoin(DEFAULT_WPT_FYI, METADATA_URL)
            _log.info("Loading WPT metadata from %s", url)
            self._metadata_data = self._json_fetcher(url)
            _log.debug("Loaded WPT metadata")
        return self._metadata_data

    def category_for_focus_area(self, year: int, focus_area: str) -> str:
        year_key = str(year)
        if year_key not in self.interop_data:
            raise InteropDataError(f"Unknown year: {year}")

        by_name = {
            v["description"]: k
            for k, v in self.interop_data[year_key]["focus_areas"].items()
        }

        categories = self.interop_data[year_key]["focus_areas"].keys()

        assert len(categories) == len(
            by_name
        ), "duplicate descriptions should not exist"

        if focus_area not in by_name:
            raise InteropDataError(f"Unknown focus area: {focus_area}")

        category = by_name[focus_area]
        assert isinstance(category, str)
        return category

    def categories_for_year(
        self,
        year: int,
        *,
        only_active: bool = True,
        use_interop_scoring_categories: bool = False,
    ) -> Set[str]:
        if only_active and use_interop_scoring_categories:
            raise InteropDataError(
                "Cannot select only active categories when using category data"
            )

        year_key = str(year)

        if use_interop_scoring_categories:
            if year_key not in self.category_data:
                raise InteropDataError(f"Unknown year: {year}")

            return {i["name"] for i in self.category_data[year_key]["categories"]}

        if year_key not in self.interop_data:
            raise InteropDataError(f"Unknown year: {year}")

        return {
            key
            for key, value in self.interop_data[year_key]["focus_areas"].items()
            if not only_active or value["countsTowardScore"]
        }

    def labels_for_categories(
        self, year: int, *, use_interop_scoring_labels: bool = False
    ) -> Mapping[str, Set[str]]:
        year_key = str(year)

        if use_interop_scoring_labels:
            if year_key not in self.category_data:
                raise InteropDataError(f"Unknown year: {year}")

            return {
                v["name"]: set(v["labels"])
                for v in self.category_data[year_key]["categories"]
            }

        if year_key not in self.category_data:
            raise InteropDataError(f"Unknown year: {year}")

        return {
            k: set(v["labels"])
            for k, v in self.interop_data[year_key]["focus_areas"].items()
        }

    def discover_years_from_data(
        self, *, use_interop_scoring_categories: bool = False
    ) -> Set[int]:
        """Extract all years available in interop_data or category_data.

        Returns a set of year integers found in the data source.
        """
        if use_interop_scoring_categories:
            data = self.category_data
        else:
            data = self.interop_data

        # Extract year keys and convert to integers
        return {int(year_key) for year_key in data.keys() if year_key.isdigit()}

    def tests_for_labels(self) -> Mapping[str, Set[str]]:
        rv = defaultdict(set)
        for test, metadata in self.metadata_data.items():
            if test.endswith("/*"):
                test = test[:-1]
            for meta_item in metadata:
                if meta_item.get("label"):
                    rv[meta_item["label"]].add(test)
        return rv


def find_wpt_tests(
    *,
    years: Optional[Sequence[int]] = None,
    categories: Optional[Sequence[str]] = None,
    focus_areas: Optional[Sequence[str]] = None,
    labels: Optional[Sequence[str]] = None,
    only_active: bool = True,
    use_interop_scoring_categories: bool = False,
    use_interop_scoring_labels: bool = False,
    include_unknown_years: bool = True,
    today: Optional[date] = None,
    finder: Optional[LabelledTestFinder] = None,
) -> Sequence[str]:
    """Find the WPT tests which are part of the given Interop selection.

    Focus areas resolve to categories, categories resolve to labels, and labels
    resolve to tests. Specifying no focus areas, categories or labels finds all
    of the tests in the given (or currently active) Interop years.

    :return List[str]: WPT test URLs, e.g. "/css/css-scroll-snap/snap-area-overflow-boundary.html"
    """

    # Canonicalize these to empty lists:
    years = list(years) if years is not None else []
    focus_areas = list(focus_areas) if focus_areas is not None else []

    # This are mutable things we might add to:
    search_labels = list(labels) if labels is not None else []
    search_categories = list(categories) if categories is not None else []

    if today is None:
        today = datetime.now(timezone.utc).date()

    if finder is None:
        finder = LabelledTestFinder()

    # Searching for labels alone doesn't need any Interop data, so don't fetch it
    # (or complain about the years it contains) unless the years actually matter.
    if years or focus_areas or search_categories or not search_labels:
        years = _resolve_years(
            finder,
            years,
            today=today,
            needs_default=bool(focus_areas or search_categories or not search_labels),
            include_unknown_years=include_unknown_years,
            use_interop_scoring_categories=use_interop_scoring_categories,
        )

    if len(years) > 1 and (focus_areas or search_categories):
        _log.warning(
            "Multiple years specified, may have surprising results with focus areas and categories"
        )

    # Nothing specified, default to everything in the year:
    if not search_categories and not focus_areas and not search_labels:
        for year in years:
            _log.info(
                "No categories or focus areas specified, "
                "defaulting to all tests in Interop %d",
                year,
            )
            search_categories.extend(
                finder.categories_for_year(
                    year,
                    only_active=only_active,
                    use_interop_scoring_categories=use_interop_scoring_categories,
                )
            )

    if focus_areas:
        for year in years:
            _log.debug(
                "Finding the categories which make up Interop %d focus areas: %s",
                year,
                ", ".join(focus_areas),
            )
            search_categories.extend(
                finder.category_for_focus_area(year, focus_area)
                for focus_area in focus_areas
            )

    if search_categories:
        for year in years:
            _log.debug(
                "Finding the labels which make up Interop %d categories: %s",
                year,
                ", ".join(search_categories),
            )
            search_labels.extend(
                set().union(
                    *(
                        v
                        for k, v in finder.labels_for_categories(
                            year, use_interop_scoring_labels=use_interop_scoring_labels
                        ).items()
                        if k in search_categories
                    )
                )
            )

    if not search_labels:
        raise InteropDataError("We cannot find tests without any labels to search for")

    _log.debug("Finding tests with labels: %s", ", ".join(search_labels))
    tests = set().union(
        *(v for k, v in finder.tests_for_labels().items() if k in search_labels)
    )

    return sorted(tests, key=lambda x: x.split("/"))


def _resolve_years(
    finder: LabelledTestFinder,
    years: Sequence[int],
    *,
    today: date,
    needs_default: bool,
    include_unknown_years: bool,
    use_interop_scoring_categories: bool,
) -> Sequence[int]:
    """Validate the given Interop years, defaulting to the active ones if needed."""

    all_interop_years: dict = {interop.year: interop for interop in known_interop_years}

    if include_unknown_years:
        discovered_years = finder.discover_years_from_data(
            use_interop_scoring_categories=use_interop_scoring_categories
        )
        for year in discovered_years - all_interop_years.keys():
            # Each year has typically run from early Feb to mid Feb, so let's assume Feb
            # 1 till the last day of the following Feb.
            all_interop_years[year] = InteropYear(
                start_date=date(year, 2, 1),
                end_date=date(year + 1, 3, 1) - timedelta(days=1),
            )
            _log.warning(
                "Year %d found in data sources but not in known years list. "
                "Assuming it runs from %s to %s.",
                year,
                all_interop_years[year].start_date,
                all_interop_years[year].end_date,
            )

    for year in years:
        try:
            interop_year = all_interop_years[year]
        except KeyError:
            _log.warning(
                "Interop %d is unknown, only the following years are known: %s",
                year,
                ", ".join(str(y) for y in sorted(all_interop_years)),
            )
            continue

        if interop_year.start_date > today:
            _log.warning(
                "Interop %d is yet to launch; it will launch on %s",
                interop_year.year,
                interop_year.start_date,
            )

        if interop_year.end_date < today:
            _log.warning(
                "Interop %d has ended with its results frozen since %s",
                interop_year.year,
                interop_year.end_date,
            )

    if not years and needs_default:
        years = sorted(
            {
                item.year
                for item in all_interop_years.values()
                if item.start_date <= today <= item.end_date
            }
        )
        _log.info(
            "No years specified, defaulting to active Interop years: %s",
            ", ".join(str(y) for y in years),
        )

    return years
