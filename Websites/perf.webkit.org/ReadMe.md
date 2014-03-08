# Concepts

## Platform

A platform is an environmental configuration under which performance tests run. This is typically
an operating system such as Lion, Mountain Lion, or Windows 7.

## Builder

A builder is a physical machine that submits a result of one or more tests to one or more platforms.
Each builder should have a password it uses to submit the results to this application, and it may also
have a URL associated with it.

## Build

A build is a single run of tests on a given builder. It's possible for a single build to have ran multiple
tests on multiple platforms.

## Test Metric

A test metric is a type of measurement a test makes. A single test may measure multiple metrics such as
Time (ms), Malloc (bytes), and JSHeap (bytes). The mapping from metrics to units is a function
(in mathematical sense).

## Test Configuration

A test configuration is a combination of a test metric, a platform, and a configuration type: "current",
"baseline", or "target". With metric, configuration creates a three-level tree structure under a test as follows:

- MyTest (Test 1)
    - Time (Metric 1)
        - Lion : current (Configuration 1)
        - Lion : baseline (Configuration 2)
        - Lion : target (Configuration 3)
        - Mountain Lion : current (Configuration 4)
        - Mountain Lion : baseline (Configuration 5)
        - Mountain Lion : target (Configuration 6)
    - Malloc (Metric 2)
        - Lion : current (Configuration 7)
        - Lion : baseline (Configuration 8)
        - Lion : target (Configuration 9)
        - Mountain Lion : current (Configuration 10)
        - Mountain Lion : baseline (Configuration 11)
        - Mountain Lion : target (Configuration 12)
- AnotherTest (Test 2)
    - Time (Metric 3)
        - Lion : current (Configuration 13)
        - Mountain Lion : current (Configuration 14)

## Run and Iteration

A run is a ordered list of values obtained for a single configuration on a single build. For example, a Lion
builder may execute MyTest 10 times, i.e. 10 iterations, and create a single run after computing the arithmetic
mean of 10 values obtained in this process. Each run has associated iterations, which represents an individual
measurement of the same configuration (of a single test metric) in the run.

## Aggregation and Aggregator

Aggregation is a process by which a test with child tests synthetically generates results for itself using
results of sub tests. For example, we may have a page loading test (PageLoadingTest), which loads
www.webkit.org and www.mozilla.org as follows:

- PageLoadingTest (Test 1)
    - www.webkit.org (Test 2)
    - www.mozilla.org (Test 3)

(Note that PageLoadingTest, www.webkit.org, and www.mozilla.org each has its own metrics and configurations,
which are not shown here.)

Then results for a metric, e.g. Time, of PageLoadingTest could be generated from results of the same metric in
subtests, namely www.webkit.org and www.mozilla.org. The process is called "aggregation", and the exact nature of
the aggregation is defined in terms of an aggregator. All aggregators are written in JavaScript.

The aggregator for arithmetic mean could be implemented as:
    
    values.reduce(function (a, b) { return a + b; }) / values.length;

When a builder reports a result JSON to the application, the background process automatically schedules a job
to aggregate results for all tests specified in the JSON. The aggregation can also be triggered manually on
`/admin/tests`.

Reporting Results
=================

To submit the results of a new test to an instance of the app, you need the following:

 - A builder already added on `/admin/builders`
 - A script that submits a JSON payload of the supported format via a HTTP/HTTPS request to `/api/report`

JSON Format
-----------

The JSON submitted to `/api/report` should be an array of dictionaries, each of which should
contain the following key-value pairs representing a single run of tests on a single build:

- `builderName` - The name of a builder present on `/admin/builders`.
- `builderPassword` - The password associated with the builder.
- `buildNumber` - The string that uniquely identifies a given build on the builder.
- `buildTime` - The time at which this build started in **UTC** (Use ISO time format such as
   2013-01-31T22:22:12.121051). This is completely independent of timestamp of repository revisions.
- `platform` - The human-readable name of a platform such as `Mountain Lion` or `Windows 7`.
- `revisions` - A dictionary that maps a repository name to a dictionary with "revision" and optionally
   "timestamp" as keys each of which maps to, respectively, the revision in **string** associated with
   the build and the times at which the revision was committed to the repository respectively.
   e.g. `{"WebKit": {"revision": "123", "timestamp": "2001-09-10T17:53:19.000000Z"}}`
- `tests` - A dictionary that maps a test name to a dictionary that represents a test. The value of a test
   itself is a dictionary with the following keys:
    - `metrics` - A dictionary that maps a metric name to a dictionary of configuration types to an array of
      iteration values. e.g. `{"Time": {"current": [629.1, 654.8, 598.9], "target": [544, 585.1, 556]}}`
      When a metric represents an aggregated value, it should be an array of aggregator names instead. e.g.
      `{"Time": ["Arithmetic", "Geometric"]}` **This format may change in near future**.
    - `url` - The URL of the test. This value should not change over time as only the latest value is stored
        in the application.
    - `tests` - A dictionary of tests; the same format as this dictionary.

A sample JSON:

[{
    "buildNumber": "651",
    "buildTime": "2013-01-31T22:22:12.121051",
    "builderName": "bot-111",
    "builderPassword": "********",
    "platform": "Mountain Lion",
    "revisions": {
        "OS X": {
            "revision": "10.8.2"
        },
        "WebKit": {
            "revision": "141469",
            "timestamp": "2013-01-31T20:55:15.452267Z"
        }
    },
    "tests": {
        "PageLoadingTest": {
            "metrics": {
                "Time": [
                    "Arithmetic",
                    "Geometric"
                ]
            },
            "tests": {
                "webkit.org": {
                    "metrics": {
                        "Time": {
                            "current": [
                                629.1,
                                654.8,
                                598.9
                            ]
                        }
                    }
                },
                "url": "http://www.webkit.org/"
            }
        }
    }
}]

FIXME: Add a section describing how the application is structured.
