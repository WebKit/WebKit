# EWS Flake Dashboard

Does EWS's flake detection stop authors being blamed for failures that were not theirs?

The WebKit tree is the canonical copy of this tool. The former standalone repository it was vendored from is no longer the source of truth — changes belong here, under `Tools/CISupport/ews-dashboard/`.

EWS records every test failure and flake it sees to results.webkit.org, then consults that history before blaming a change. This dashboard measures whether that helps. It answers one question on the front page — what share of failing builds showed an author a failure that main already fails — and links out to results.webkit.org and EWS for everything else, because both already render this data better than a second copy would.

![The Explore page: the failing builds in the picked queues, and one build's author-visible failures](docs/explore.png)

Behind the front page, Explore holds two panes side by side — the failing builds in the queues picked from the header dropdown, then one build's author-visible failures with what main says about each and why that verdict was reached. Every filter is a link, so any view is a URL you can send someone. A legend at the foot of the page glosses every build-state chip with a live count and a link back to the pane narrowed to it where one is honestly obtainable from the same scoped query the pane above already ran; the test-verdict table beside it carries no count column.

Tests holds the convicted-test table: one row per test, with every flake type it was convicted under, how many times, on how many queues, and when it was last seen. Clicking a test opens a drilldown beside the table listing every conviction of that one test — its build, queue, flake type and when. The table is filtered and ordered by two repeatable query arguments, which the *Filter and sort* disclosure in its header writes for you: `f.tests=<column>:<condition>:<value>` and `s.tests=<column>:<asc|desc>`. Both are read in the order they are written, so two filters both apply and two sorts are a primary and a secondary key — `?f.tests=test:has:editing&f.tests=convictions:ge:3&s.tests=last_seen:desc&s.tests=test:asc`. The columns and the conditions each one takes are listed in the disclosure and defined in `Tools/CISupport/ews-dashboard/ews_dashboard/analysis/filters.py`, which is the only place a request can name a column at all; a clause it cannot read is ignored and named on the page rather than refusing the request, since a URL is the whole of this page's state. A legend at the foot of the page glosses each flake type, with a conviction count for the window and a link to the table narrowed to it.

## Running it

```
python3 -m ews_dashboard.db                 # create the database
python3 -m scripts.refresh                  # ingest builds and classify them (slow, needs network)
python3 -c "import ews_dashboard, flask.cli; flask.cli.main()" --app ews_dashboard.web.app:create_app run
```

Run these from `Tools/CISupport/ews-dashboard/`. `python3 -m flask` will not work here: `-m flask` resolves and imports `flask` before anything imports `ews_dashboard`, so `ews_dashboard`'s `AutoInstall` registration never runs. Flask and its dependencies are pinned and installed automatically by webkitcorepy's `AutoInstall` the first time `ews_dashboard` is imported — there is no `pip install` step and no `requirements.txt`; see `Tools/CISupport/ews-dashboard/ews_dashboard/__init__.py` for the registration.

No credentials of any kind. Both APIs it reads are public and it only ever issues GETs, so there is nothing to configure and nothing to leak. `EWS_DASHBOARD_DATABASE` overrides where the sqlite file lives.

`scripts/refresh.py` is the only thing that touches the network. It defaults to the widest window the pages offer, 90 days, so a verdict exists for everything a reader can ask to see; `--days` narrows it. The web app reads the database and nothing else, which is why a page cannot hang on a slow results.webkit.org query and why every page shows how old its numbers are.

## How the metric is defined

A build's author-visible failures are the tests that failed in both the first run and the rerun and did not fail on a clean tree — the set behind "Found N new test failures". Each is looked up on main at the commit the change was rebased onto:

- **pre-existing** — main passes it 80% of the time or less, so the change is not the cause
- **real** — main passes it reliably, so the change is the likely cause
- **undetermined** — nothing recorded for that test in that configuration

Every rate on the pages is a floor, not a verdict. Main does not contain the change, so a test that is reliable on main and genuinely broken by the change looks identical to one that flaked once during this build. Read a trend, not a single build.

The filtered failure lists EWS publishes — `first_run_failures_filtered` and `second_run_failures_filtered`, which hold what the author was actually shown — only exist on builds from after the EWS deployments `config.py` dates: 2026-08-14 for the write path and 2026-08-25 for the read path, the first build carrying a results-db flakiness property. A layout-test build from before those dates carries neither, because the same step sets the filtered lists and the flakiness properties together, so any window reaching back past them mixes two schemes — `ingest.py` prefers the filtered lists per build and falls back to the raw ones.

## What main said afterwards

A conviction is a failure this dashboard excused as pre-existing. Once its pull request has landed, `analysis/escapes.py` asks main whether that excuse held, over one window either side of the landing and in the same configuration, and stores one verdict per conviction: **ESCAPED** (main never failed the test before the landing and failed it after), **FAILS_ON_MAIN** (main was already failing it, so the failure is main's own), **CONTAINED** (main ran it after the landing and never failed it), **NO_RUNS**, **NO_BASELINE** and **TREE_DIVERGED** for the convictions main answered nothing about.

How hard an escape failed is not a verdict of its own: the raw rate is read off the stored counts, so an escape whose failures after the landing reach `ESCAPE_FAILURE_PCT` of the runs is strong and one below that rests on few failures. The escapes table also shows escape strength, the lower end of the 90% Wilson interval on that same ratio, which sinks thin evidence and so ranks escapes against each other rather than by the raw rate — it is a ranking, not the STRONG/RARE split, which stays the raw rate against `ESCAPE_FAILURE_PCT`. Whether main is *still* failing the test is a separate question, asked of the escapes alone over a fresh `CURRENCY_DAYS` window and re-asked once a day. It has four answers, not two: still failing, recovered, not run lately when main ran the test no times in the window, and unchecked when nothing has asked yet — the last two are absences of evidence, and neither of them is a recovery. The escapes table shows this as current damage, the raw `recent_failed / recent_runs` from that check, blank rather than zero when nothing has asked or main ran nothing about it.

## Layout

```
Tools/CISupport/ews-dashboard/
  ews_dashboard/
    schema.sql      tables, views, and the invalidation rule for every cache
    db.py           connections and forward-only migrations
    config.py       thresholds, rule names, and the dated list of EWS deployments
    suites.py       which builders are read, and how each publishes its failure lists
    buildbot.py     EWS's Buildbot API
    results.py      results.webkit.org history, cached, including negative caching
    ingest.py       builds and flakiness verdicts into the database
    analysis/       false_positive, convictions, escapes, filters, trend, freshness
    web/            Flask app, links out, SVG chart geometry, templates
  scripts/refresh.py
  tests/
```

There is no CDN, no build step and no npm. The chart is server-generated SVG, and `Tools/CISupport/ews-dashboard/ews_dashboard/web/static/dashboard.css` is the whole stylesheet, so the pages render from a checkout with no network. `Tools/CISupport/ews-dashboard/ews_dashboard/web/static/dashboard.js` is the one script, a plain file with no framework that enhances the tests page's filter/sort chips and the queue picker's checkbox tree — ticking a group or version there also ticks what it contains on screen, and narrows the submitted query to that parent's own value rather than every builder it happens to contain today; every page works the same, one request per click, with it blocked.

Known gaps and planned work are tracked in [`Tools/CISupport/ews-dashboard/docs/open-work.md`](docs/open-work.md), not an issue tracker, so a reader looking at the code can see in one place what is not done yet and why.

## Tests

```
python3 Tools/CISupport/run-tests ews-dashboard
```

`check-webkit-style` covers this tool the same way it covers the rest of `Tools/CISupport/`.
