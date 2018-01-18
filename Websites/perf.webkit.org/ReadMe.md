# WebKit Performance Dashboard

The WebKit performane dashboard is a website to track various performance metrics of WebKit: https://perf.webkit.org/

## 1. Checking Out the Code and Installing Required Applications

The instructions assume you're using macOS as the host server and installing this application at `/Volumes/Data/perf.webkit.org`.

You can choose between using Server.app or install the required tools separately.

1. Install Server.app but do **NOT** launch/open it. (if you don't want to use Server.app, install PostgreSQL: http://www.postgresql.org/download/macosx/)
2. Install node.
3. Install Xcode with command line tools (only needed for svn)
4. `svn co https://svn.webkit.org/repository/webkit/trunk/Websites/perf.webkit.org /Volumes/Data/perf.webkit.org`
5. Inside `/Volumes/Data/perf.webkit.org`, run `npm install pg` and `mkdir -m 755 public/data/`

## 2. Testing Local UI Changes with Production Data

The front end has the capability to pull data from a production server without replicating the database locally on OS X Yosemite and later.
To use this feature to test local UI changes, modify `config.json`'s `remoteServer` entry so that "remoteServer.url" points to your production server,
and "remoteServer.basicAuth" specifies the username and the password that is used by the production sever.

Remove "basicAuth" entry for production servers that doesn't require a basic authentication (e.g. perf.webkit.org).

```json
{
    "url": "http://perf.webkit.org",
    "basicAuth": {
        "username": "webkitten",
        "password": "webkitten's secret password"
    }
}
```

Then run `tools/remote-cache-server.py start`. This launches a httpd server on port 8080.

The script caches remote server's responses under `public/data/remote-cache` and never revalidates them (to allow offline work).
If you needed the latest content, delete caches stored in this directory by running `tools/remote-cache-server.py reset`.

## 3. Running Tests

There are three kinds of tests in directories of the same name: `unit-tests`, `server tests`, and `browser-tests`.

 - `unit-tests`: These tests various models and common JS code used in v3 UI and tools. They mock JSON APIs and model object.
 - `server-tests`: Server tests use a real Apache server in accordance with `testServer` in `config.json` and a Postgres database by the name of `testDatabaseName` specified in `config.json`. They're functional tests and may test both the backend database schema, PHP, and corresponding front-end code although many of them directly queries and modifies the database.
 - `browser-tests`: These are tests to be ran inside a Web browser, and tests v3 UI's interaction with browser's DOM API.

To run `unit-tests` and `server-tests`, simply run `./tools/run-tests.py` after installing dependencies (1) and configuring the PostgreSQL (4).
If you've previously setup a remote cache server (3), then you have to stop the server before running the tests.

To run `browser-tests`, open `browser-tests/index.html` inside a Web browser.

## 4. Configuring PostgreSQL

Run the following command to setup a Postgres server at `/Volumes/Data/perf.webkit.org/PostgresSQL` (or wherever you'd prefer):
`python ./tools/setup-database.py /Volumes/Data/perf.webkit.org/PostgresSQL`

It automatically retrieves the database name, the username, and the password from `config.json`. Create one from `config.json.sample`

### Starting PostgreSQL

The setup script automatically starts the database but you may need to run the following command to manually start the database after reboot.

- Starting the database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_ctl -D /Volumes/Data/perf.webkit.org/PostgresSQL -l /Volumes/Data/perf.webkit.org/PostgresSQL/logfile -o "-k /Volumes/Data/perf.webkit.org/PostgresSQL" start`
- Stopping the database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_ctl -D /Volumes/Data/perf.webkit.org/PostgresSQL -l /Volumes/Data/perf.webkit.org/PostgresSQL/logfile -o "-k /Volumes/Data/perf.webkit.org/PostgresSQL" stop`

### Initializing the Database

Run `database/init-database.sql` in psql as `webkit-perf-db-user`:
`/Applications/Server.app/Contents/ServerRoot/usr/bin/psql webkit-perf-db -h localhost --username webkit-perf-db-user -f init-database.sql`

### Making a Backup and Restoring

Use `pg_dump` and `pg_restore` to backup and restore the database. If you're replicating the production database for development purposes, you may consider excluding `run_iterations` table, which takes up 2/3 of the storage space, to reduce the size of the database for your local copy. Adjust the number of concurrent processes to use by `--jobs` and adjust the compression level by `--compress` (0 is no compression, 9 is most compressed).

- Making the fullbackup of the database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_dump -h localhost webkit-perf-db --format=directory --jobs=4 --no-owner --compress=7 --file=<path to backup directory>`

- Making an abridged backup without `run_iterations` table: `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_dump -h localhost webkit-perf-db --format=directory --jobs=4 --no-owner --compress=7 --exclude-table=run_iterations --file=<path to backup directory>`

- Restoring the database: `/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_restore --format=directory --jobs=4 --no-owner --host localhost --username=webkit-perf-db-user --dbname=webkit-perf-db <path to backup directory>`

## 5. Configuring Apache

### Instructions if you've never launched Server.app as instructed

 - Edit `/private/etc/apache2/httpd.conf`

     1. Change DocumentRoot to `/Volumes/Data/perf.webkit.org/public/`
     2. Uncomment `LoadModule php5_module libexec/apache2/libphp5.so`
     3. Uncomment `LoadModule rewrite_module libexec/apache2/mod_rewrite.so`
     4. Uncomment `LoadModule deflate_module libexec/apache2/mod_deflate.so`

 - In Mavericks and later, copy php.ini to load pdo_pgsql.so pgsql.so.
    `sudo cp /Applications/Server.app/Contents/ServerRoot/etc/php.ini /etc/`
 - In El Capitan and later, you may need to comment out the `LockFile` directive in `/private/etc/apache2/extra/httpd-mpm.conf`
   as the directive has been superseded by `Mutex` directive.

### Instructions if you've accidentally launched Server.app

 - Enable PHP web applications
 - Go to Server Website / Store Site Files In, change it to `/Volumes/Data/perf.webkit.org/public/`
 - Go to Server Website / Edit advanced settings, enable Allow overrides using .htaccess files
 - httpd config file is located at `/Library/Server/Web/Config/apache2/sites/0000_any_80.conf` (and/or 0000_any_`PORT#`.conf)

### Starting Apache

You can use apachectl to start/stop/restart apache server from the command line:

- Starting httpd: `sudo apachectl start`
- Stopping httpd: `sudo apachectl stop`
- Restarting httpd: `sudo apachectl restart`

The apache logs are located at `/private/var/log/apache2`.

### Production Configurations

 1. Update ServerAdmin to your email address
 2. Add the following directives to enable gzip:

        <IfModule mod_deflate.c>
            AddOutputFilterByType DEFLATE text/html text/xml text/javascript application/javascript text/plain application/json application/xml application/xhtml+xml
        </IfModule>

 3. Add the following directives to enable zlib compression and MultiViews on DocumentRoot (perf.webkit.org/public):

        Options Indexes MultiViews
        php_flag zlib.output_compression on

 4. Increase the maximum upload size as needed for accepting custom roots:

        <IfModule php5_module>
        php_value upload_max_filesize 100M
        php_value post_max_size 100M
        </IfModule>

### Protecting the Administrative Pages to Prevent Execution of Arbitrary Code

By default, the application gives the administrative privilege to everyone. Anyone can add, remove, or edit tests,
builders, and other entities in the database.

We recommend protecting the administrative pages via Digest Auth on https connection.

Generate a password file via `htdigest -c <path> <realm> <username>`, and then create admin/.htaccess with the following directives
where `<Realm>` is replaced with the realm of your choice, which will be displayed on the username/password input box:

```
AuthType Digest
AuthName "<Realm>"
AuthDigestProvider file
AuthUserFile "<Realm>"
Require valid-user
```

## 6. Concepts

 - **Test** - A test is a collection of metrics such as frame rate and malloced bytes. It can have a children and a parent and form a tree of tests.
 - **Metric** - A specific metric of a test such as frame rate, runs per second, and time. The list of supported metrics are:
   - *FrameRate* - Frame rate per second
   - *Runs* - Runs per second
   - *Time* and *Duration* - Duration measured in milliseconds
   - *Size* - Bytes
   - *Score* - Unit-less scores for benchmarks. Unit is pt or points.

   Metrics can also be an aggreagte of child tests' metrics of the same name. For example, Speedometer benchmark's Time metric is the sum total of subtests' time. The list of supported aggregation types are:
   - *Arithmetic* - [The arithmetic mean](https://en.wikipedia.org/wiki/Arithmetic_mean)
   - *Geometric* - [The geometric mean](https://en.wikipedia.org/wiki/Geometric_mean)
   - *Harmonic* - [The harmonic mean](https://en.wikipedia.org/wiki/Harmonic_mean)
   - *Total* - [The sum](https://en.wikipedia.org/wiki/Summation)

 - **Platform** - A platform is an environmental configuration under which performance tests run. This is typically an operating system such as Lion, Mountain Lion, or Windows 7, or a combination of an operating system, a particular device, and a configuration: e.g. macOS Sierra MacBookAir7,1.
 - **Repository** - A repository refers to the name of a collection of software whose verions or revisions need to be associated with a particular data point submitted to the dashboard. For example, WebKit is a repository because each data point submitted to the dashboard has to identify its WebKit revision. Operating systems such as macOS could also be considered as a repository if its version may change over time if an operating system may get updated across data points.
 - **Commit** - A commit is a specific revision or a version of a repository. e.g. r211196 of WebKit.
 - **Committer** - A committer is the author of the change in a given repository for a specific commit. e.g. the author of r211196 in WebKit is Ryosuke Niwa.
 - **Builder** - Like a builder in buildbot, a builder submits data points to the dashboard in terms of a sequence of builds. For example, a builder named "El Capitan MacBookAir7,1" may submit data points for benchmarks ran on MacBookAir7,1 with El Capitan.
 - (Build) **Slave** - Like a buildlsave in buildbot, a slave is a physical machine that submit data points to the dashboard. e.g. it could be a bot205 which submits data points as either "El Capitan MacBookAir7,1" or "Sierra MacBookAir7,1".
 - **Build** - A build represents a set of data points reported by a specific builder. e.g. "El Capitan MacBookAir7,1" may report Speedometer score along with its subtests' results. All those data points being to a single build. This is a different concept from a build of software. A single build of WebKit, for example, could be ran on multiple builders (e.g. one MacBookAir and another MacBookPro) to generate two builds in the dashboard.
 - **Bug Tracker** - A bug or an issue tracking tool such as Bugzilla and Radar.
 - **Bug** - A bug number associated with a particular bug tracker.
 - **Analysis Task** - An analysis task is created to analyze a progression or a regression across a range of data points for a specific test metric on a specific platform. Each analysis task can have multiple test groups.
 - **Test Group** - A specific set of A/B testing tasks. Each group may have multiple build requests for each set A and B.
 - **Build Request** - A specific testing task in a test group.

See [init-database.sql](init-database.sql) for the complete database schema.

## 7. Data Models for Test Results

In the performance dashboard, each test can have a parent test or arbitrary many child tests or subtests. Each test can then have multiple metrics to measure a specific unit'ed value. For example, Speedometer benchmark has the top level score, which is computed by the total time of running subtests. As such, the top level test has two metrics: *Score* and *Time* which is the aggregated total sum of *Time* metrics of the subtests:

**Speedometer** (A test)
 + *Score* (A metric of "Speedometer")
 + *Time : Total* (An aggregated metric of "Speedometer"; total sum of "Time" metrics of all subtests)
 + **AngularJS-TodoMVC** (A subtest of "Speedometer")
    - *Time* (A metric of "AngularJS-TodoMVC")
 + **BackboneJS-TodoMVC** (A subtest of "Speedometer")
    - *Time* (A metric of "BackboneJS-TodoMVC")
 + ...

Since each test metric can be measured on arbitrarily platforms (e.g. MacBookAir7,1 on macOS Sierra), the dashboard supports showing the *baseline* results (e.g. benchmark scores on Safari 10) in addition to the results from the *current* sofware (the trunk WebKit build), we use a triple (test metric, platform, type) called a *test configuration* to group a collection of data points reported to the dashboard.

Then each test configuration has arbitrary *test runs*. A test run represents the score or more broadly the result of a single test metric on a specific platform at a particular time. For example, running Speedometer once and reporting the results to the performance dashboard results in the creation of a test run for each of *Score* and *Time : Total* metrics as well as *Time* metrics of all subtests (e.g. AngularJS-TodoMVC). Each test run can then have arbitrarily many *iteration values* which are indivisual measurement of some test metric within the benchmark. For example, Speedometer runs the same test twenty times and uses the average time and the score of those twenty iterations to compute the final score. Each one of twenty iterations constitutes a single iteration value.

Each *test run* are related to one another via *builds* which is uniquely identified by its *builder* and a *build number*. For example in the following sample data points for Speedometer, two test runs 789 and 791 are reported by a build 1001, and test runs 876 and 878 are reported by build 1002. Because Speedometer's total score is computed using the time took to run subtests, a builder reported both values in a single build as expected.

(Speedometer's Score, Sierra MacBookAir7,1, "current")
 + Test run 789 - Mean: 154, Build: 1001
   + Iteration 1: 150
   + Iteration 2: 159
   + ...
 + Test run 876 - Mean: 153, Build: 1002
   + Iteration 1: 152
   + Iteration 2: 157
   + ...

(AngularJS-TodoMVC's Time, Sierra MacBookAir7,1, "current")
 + Test run 791 - Mean: 373, Build: 1001
   + Iteration 1: 450
   + Iteration 2: 380
   + ...
 + Test run 878 - Mean: 380, Build: 1002
   + Iteration 1: 448
   + Iteration 2: 395
   + ...

## 8. Reporting Results

To submit a new *build*, or a set of data points to an instance of the performance dashboard, you need to do the following:

 - Add a slave on `/admin/slaves` if the slave used in the report has not already been added.
 - Make a HTTP POST request to `/api/report`.

### Format of Results JSON

The JSON submitted to `/api/report` should be an array of dictionaries, and each dictionary should must contain the following key-value pairs representing a single run of tests and its subtests on a single build:

- `builderName` - The name of a builder. A single slave may submit to multiple builders.
- `slaveName` - The name of a slave present on `/admin/slaves`.
- `slavePassword` - The password associated with the slave.
- `buildNumber` - The string that uniquely identifies a given build on the builder.
- `buildTime` - The time at which this build started in **UTC** (Use ISO time format such as `2013-01-31T22:22:12.121051`). This is completely independent of timestamp of repository revisions.
- `platform` - The human-readable name of a platform such as `Mountain Lion` or `Windows 7`.
- `revisions` - A dictionary that maps a repository name to a dictionary with "revision" and optionally "timestamp" as keys each of which maps to, respectively, the revision in **string** associated with the build and the times at which the revision was committed to the repository respectively. e.g. `{"WebKit": {"revision": "123", "timestamp": "2001-09-10T17:53:19.000000Z"}}`
- `tests` - A dictionary that maps a test name to a dictionary that represents a test. The value of a test
   itself is a dictionary with the following keys:
    - `metrics` - A dictionary that maps a metric name to a dictionary of configuration types to an array of iteration values. e.g. `{"Time": {"current": [629.1, 654.8, 598.9], "target": [544, 585.1, 556]}}`
      When a metric represents an aggregated value, it should be an array of aggregator names instead. e.g. `{"Time": ["Arithmetic", "Geometric"]}` **This format may change in near future**.
    - `url` - The URL of the test. This value should not change over time as only the latest value is stored in the application.
    - `tests` - A dictionary of tests; the same format as this dictionary.

In the example below, we have the top-level test named "PageLoadTime". It measures two metrics: `Time` and `FrameRate`.
`Time` metric is the arithmetic mean of each subtest's `Time` metric (webkit.org and www.w3.org).
The computed arithmetic means are `[965.6, 981.35, 947.15]` in this case.
The test also reports `FrameRate` but this metric is measured only for the entire suite not per each subtest.

```json
[{
    "buildNumber": "651",
    "buildTime": "2013-01-31T22:22:12.121051",
    "builderName": "Trunk Mountain Lion Performance Tests",
    "slaveName": "bot-111",
    "slavePassword": "somePassword",
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
        "PageLoadTime": {
            "metrics": {
                "Time": ["Arithmetic"],
                "FrameRate": {
                    "current": [31, 24, 29]
                }
            },
            "tests": {
                "webkit.org": {
                    "metrics": {
                        "Time": {
                            "current": [629.1, 654.8, 598.9]
                        }
                    },
                    "url": "https://webkit.org/"
                },
                "www.w3.org": {
                    "metrics": {
                        "Time": {
                            "current": [1302.1, 1307.9, 1295.4]
                        }
                    },
                    "url": "https://www.w3.org/"
                }
            }
        }
    }
}]
```
