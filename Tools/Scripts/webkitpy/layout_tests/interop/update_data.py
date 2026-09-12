# Copyright (C) 2026 Apple Inc. All rights reserved.
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
#     * Neither the name of Apple Inc. nor the names of its
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

"""Regenerate an interop data file from the live wpt.fyi metadata.

The Interop focus areas for a year are defined in wpt.fyi's interop-data.json
(served at /static/interop-data.json). Each focus area carries a human-readable
description and the test-level metadata labels (e.g. "interop-2026-scroll-snap")
that mark its tests. This tool reads that file, then asks the wpt.fyi search API
which tests carry each label, and writes the interop-<year>.json consumed by
--show-interop-score.

Note: the focus-area labels are NOT available through /api/labels -- that
endpoint only lists run labels (master, stable, product names, user:*).
"""

import argparse
import json
import sys

from urllib.request import Request, urlopen
from urllib.parse import urlencode

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.layout_tests.interop import interop_score

INTEROP_DATA_URL = "https://wpt.fyi/static/interop-data.json"
RUNS_URL = "https://wpt.fyi/api/runs"
SEARCH_URL = "https://wpt.fyi/api/search"
DEFAULT_PRODUCTS = ["chrome", "firefox", "safari"]
DEFAULT_RUN_LABEL = "master"


def _truncate(text, limit=4000):
    if len(text) <= limit:
        return text
    return "{}... [{} more bytes]".format(text[:limit], len(text) - limit)


def _fetch_json(url, post_body=None, verbose_log=None):
    headers = {"Accept": "application/json"}
    data = None
    method = "GET"
    if post_body is not None:
        data = json.dumps(post_body).encode("utf-8")
        headers["Content-Type"] = "application/json"
        method = "POST"
    if verbose_log:
        verbose_log("> {} {}\n".format(method, url))
        if post_body is not None:
            verbose_log("> body: {}\n".format(json.dumps(post_body)))
    response = urlopen(Request(url, data=data, headers=headers))
    raw = response.read().decode("utf-8")
    if verbose_log:
        status = getattr(response, "status", None) or response.getcode()
        verbose_log("< HTTP {} ({} bytes)\n".format(status, len(raw)))
        verbose_log("< {}\n".format(_truncate(raw)))
    return json.loads(raw)


def _focus_areas(year, verbose_log=None):
    """Return the scoring focus areas for a year as a list of dicts with
    ``key`` (label minus the interop-<year>- prefix), ``name``, ``focus_label``
    (the interop-<year>-* id), and ``metadata_labels`` (the test-level labels to
    query). Sourced from wpt.fyi's interop-data.json -- the interop focus areas
    are NOT exposed through /api/labels (that only lists run labels)."""
    prefix = "interop-{}-".format(year)
    data = _fetch_json(INTEROP_DATA_URL, verbose_log=verbose_log)
    year_data = data.get(str(year))
    if not year_data:
        raise ValueError("Interop data on wpt.fyi has no entry for year {}.".format(year))

    focus_areas = year_data.get("focus_areas", {})
    areas = []
    for focus_label in sorted(focus_areas):
        entry = focus_areas[focus_label]
        if not entry.get("countsTowardScore", True):
            continue
        metadata_labels = entry.get("labels") or [focus_label]
        key = focus_label[len(prefix):] if focus_label.startswith(prefix) else focus_label
        areas.append({
            "key": key,
            "name": entry.get("description", focus_label),
            "focus_label": focus_label,
            "metadata_labels": metadata_labels,
        })
    if verbose_log:
        verbose_log("Year {} defines {} focus areas ({} count toward the score).\n".format(
            year, len(focus_areas), len(areas)))
    return areas


def _resolve_run_ids(products, run_label, verbose_log=None):
    """Resolve the latest aligned run IDs for the given products. The structured
    search API requires explicit run_ids in the request body."""
    params = [("label", run_label), ("aligned", "true"), ("max-count", "1")]
    params += [("product", product) for product in products]
    url = "{}?{}".format(RUNS_URL, urlencode(params))
    runs = _fetch_json(url, verbose_log=verbose_log)
    run_ids = [run["id"] for run in runs if "id" in run]
    if verbose_log:
        verbose_log("Resolved {} run IDs: {}\n".format(len(run_ids), run_ids))
    return run_ids


def _tests_for_labels(metadata_labels, run_ids, verbose_log=None):
    """Union of the tests carrying any of the given test-level metadata labels,
    within the given runs."""
    tests = set()
    for label in metadata_labels:
        response = _fetch_json(SEARCH_URL, post_body={"run_ids": run_ids, "query": {"label": label}},
                               verbose_log=verbose_log)
        tests.update(result["test"] for result in response.get("results", []))
    return sorted(tests)


def build_interop_data(year, products, run_label, log, verbose_log=None):
    areas = _focus_areas(year, verbose_log=verbose_log)
    if not areas:
        raise ValueError("No scoring focus areas found for Interop {} on wpt.fyi.".format(year))

    run_ids = _resolve_run_ids(products, run_label, verbose_log=verbose_log)
    if not run_ids:
        raise ValueError("No aligned runs found for products {} (run label {}).".format(
            ", ".join(products), run_label))

    log("Found {} focus areas for Interop {}; using {} aligned runs.\n".format(len(areas), year, len(run_ids)))
    categories = {}
    for area in areas:
        tests = _tests_for_labels(area["metadata_labels"], run_ids, verbose_log=verbose_log)
        categories[area["key"]] = {
            "name": area["name"],
            "label": area["focus_label"],
            "labels": area["metadata_labels"],
            "tests": tests,
        }
        log("  {}: {} tests\n".format(area["focus_label"], len(tests)))

    return {
        "_comment": "Generated by Tools/Scripts/update-interop-data from wpt.fyi metadata. "
                    "Test paths are wpt.fyi-style and are mapped under imported/w3c/web-platform-tests.",
        "year": year,
        "generated": True,
        "categories": categories,
    }


def main(argv, stdout, stderr):
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("--year", type=int, default=2026, help="Interop year to fetch (default: 2026).")
    parser.add_argument("--output", help="Path to write (default: metadata/interop/interop-<year>.json).")
    parser.add_argument("--product", action="append", dest="products", metavar="PRODUCT",
                        help="Product whose latest run supplies the test set; repeatable (default: %s)."
                             % ", ".join(DEFAULT_PRODUCTS))
    parser.add_argument("--run-label", default=DEFAULT_RUN_LABEL,
                        help="wpt.fyi run label to resolve latest runs from (default: %s)." % DEFAULT_RUN_LABEL)
    parser.add_argument("-v", "--verbose", action="store_true", default=False,
                        help="Log each HTTP request and (truncated) response.")
    options = parser.parse_args(argv)

    products = options.products or DEFAULT_PRODUCTS
    filesystem = FileSystem()
    output = options.output or interop_score.default_data_path(filesystem, options.year)
    verbose_log = stderr.write if options.verbose else None

    try:
        data = build_interop_data(options.year, products, options.run_label, stderr.write, verbose_log=verbose_log)
    except Exception as error:  # Network/HTTP/JSON failures should be reported, not tracebacked.
        print("Failed to fetch interop data: {}".format(error), file=stderr)
        return 1

    filesystem.maybe_make_directory(filesystem.dirname(output))
    with open(output, "w") as output_file:
        json.dump(data, output_file, indent=4, sort_keys=True)
        output_file.write("\n")
    print("Wrote {} focus areas to {}".format(len(data["categories"]), output), file=stdout)
    return 0
