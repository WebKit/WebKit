'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const MockData = require('./resources/mock-data.js');
const TestServer = require('./resources/test-server.js');
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe('/api/manifest', function () {
    prepareServerTest(this);

    it("should generate an empty manifest when database is empty", () => {
        return TestServer.remoteAPI().getJSON('/api/manifest').then((manifest) => {
            assert.deepEqual(Object.keys(manifest).sort(), ['all', 'bugTrackers', 'builders', 'dashboard', 'dashboards',
                'elapsedTime', 'fileUploadSizeLimit', 'metrics', 'repositories', 'siteTitle', 'status', 'summaryPages', 'tests', 'triggerables']);

            assert.equal(typeof(manifest.elapsedTime), 'number');
            delete manifest.elapsedTime;

            assert.deepStrictEqual(manifest, {
                siteTitle: TestServer.testConfig().siteTitle,
                all: {},
                bugTrackers: {},
                builders: {},
                dashboard: {},
                dashboards: {},
                metrics: {},
                repositories: {},
                tests: {},
                triggerables: {},
                summaryPages: [],
                fileUploadSizeLimit: 0,
                status: 'OK'
            });
        });
    });

    const bugzillaData = {id: 1, name: 'Bugzilla', bug_url: 'https://webkit.org/b/$number', new_bug_url: 'https://bugs.webkit.org/'};
    const radarData = {id: 2, name: 'Radar'};

    it("should generate manifest with bug trackers without repositories", () => {
        return TestServer.database().insert('bug_trackers', bugzillaData).then(() => {
            return TestServer.remoteAPI().getJSON('/api/manifest');
        }).then((content) => {
            assert.deepEqual(content.bugTrackers, {1: {name: 'Bugzilla', bugUrl: 'https://webkit.org/b/$number',
                newBugUrl: 'https://bugs.webkit.org/', repositories: null}});

            let manifest = Manifest._didFetchManifest(content);
            let tracker = BugTracker.findById(1);
            assert(tracker);
            assert.equal(tracker.name(), 'Bugzilla');
            assert.equal(tracker.bugUrl(123), 'https://webkit.org/b/123');
            assert.equal(tracker.newBugUrl(), 'https://bugs.webkit.org/');
        });
    });

    it("should generate manifest with bug trackers and repositories", () => {
        let db = TestServer.database();
        return Promise.all([
            db.insert('bug_trackers', bugzillaData),
            db.insert('bug_trackers', radarData),
            db.insert('repositories', {id: 11, name: 'WebKit', url: 'https://trac.webkit.org/$1'}),
            db.insert('repositories', {id: 9, name: 'OS X'}),
            db.insert('repositories', {id: 22, name: 'iOS'}),
            db.insert('tracker_repositories', {tracker: bugzillaData.id, repository: 11}),
            db.insert('tracker_repositories', {tracker: radarData.id, repository: 9}),
            db.insert('tracker_repositories', {tracker: radarData.id, repository: 22}),
        ]).then(() => {
            return TestServer.remoteAPI().getJSON('/api/manifest');
        }).then((content) => {
            let manifest = Manifest._didFetchManifest(content);

            let webkit = Repository.findById(11);
            assert(webkit);
            assert.equal(webkit.name(), 'WebKit');
            assert.equal(webkit.urlForRevision(123), 'https://trac.webkit.org/123');

            let osx = Repository.findById(9);
            assert(osx);
            assert.equal(osx.name(), 'OS X');

            let ios = Repository.findById(22);
            assert(ios);
            assert.equal(ios.name(), 'iOS');

            let tracker = BugTracker.findById(1);
            assert(tracker);
            assert.equal(tracker.name(), 'Bugzilla');
            assert.equal(tracker.bugUrl(123), 'https://webkit.org/b/123');
            assert.equal(tracker.newBugUrl(), 'https://bugs.webkit.org/');
            assert.deepEqual(tracker.repositories(), [webkit]);

            tracker = BugTracker.findById(2);
            assert(tracker);
            assert.equal(tracker.name(), 'Radar');
            assert.deepEqual(Repository.sortByName(tracker.repositories()), [osx, ios]);
        });
    });

    it("should generate manifest with builders", () => {
        let db = TestServer.database();
        return Promise.all([
            db.insert('builders', {id: 1, name: 'SomeBuilder', password_hash: 'a',
                build_url: 'https://build.webkit.org/builders/$builderName/build/$buildNumber'}),
            db.insert('builders', {id: 2, name: 'SomeOtherBuilder', password_hash: 'b'})
        ]).then(() => {
            return TestServer.remoteAPI().getJSON('/api/manifest');
        }).then((content) => {
            assert.deepEqual(content.builders, {
                '1': {name: 'SomeBuilder', buildUrl: 'https://build.webkit.org/builders/$builderName/build/$buildNumber'},
                '2': {name: 'SomeOtherBuilder', buildUrl: null}
            });

            let manifest = Manifest._didFetchManifest(content);

            let builder = Builder.findById(1);
            assert(builder);
            assert.equal(builder.name(), 'SomeBuilder');
            assert.equal(builder.urlForBuild(123), 'https://build.webkit.org/builders/SomeBuilder/build/123');

            builder = Builder.findById(2);
            assert(builder);
            assert.equal(builder.name(), 'SomeOtherBuilder');
            assert.equal(builder.urlForBuild(123), null);
        });
    });

    it("should generate manifest with tests, metrics, and platforms", () => {
        let db = TestServer.database();
        return Promise.all([
            db.insert('tests', {id: 1, name: 'SomeTest'}),
            db.insert('tests', {id: 2, name: 'SomeOtherTest'}),
            db.insert('tests', {id: 3, name: 'ChildTest', parent: 1}),
            db.insert('tests', {id: 4, name: 'GrandChild', parent: 3}),
            db.insert('aggregators', {id: 200, name: 'Total'}),
            db.insert('test_metrics', {id: 5, test: 1, name: 'Time'}),
            db.insert('test_metrics', {id: 6, test: 2, name: 'Time', aggregator: 200}),
            db.insert('test_metrics', {id: 7, test: 2, name: 'Malloc', aggregator: 200}),
            db.insert('test_metrics', {id: 8, test: 3, name: 'Time'}),
            db.insert('test_metrics', {id: 9, test: 4, name: 'Time'}),
            db.insert('platforms', {id: 23, name: 'iOS 9 iPhone 5s'}),
            db.insert('platforms', {id: 46, name: 'Trunk Mavericks'}),
            db.insert('test_configurations', {id: 101, metric: 5, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 102, metric: 6, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 103, metric: 7, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 104, metric: 8, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 105, metric: 9, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 106, metric: 5, platform: 23, type: 'current'}),
            db.insert('test_configurations', {id: 107, metric: 5, platform: 23, type: 'baseline'}),
        ]).then(() => {
            return TestServer.remoteAPI().getJSON('/api/manifest');
        }).then((content) => {
            assert.deepEqual(content.tests, {
                "1": {"name": "SomeTest", "parentId": null, "url": null},
                "2": {"name": "SomeOtherTest", "parentId": null, "url": null},
                "3": {"name": "ChildTest", "parentId": "1", "url": null},
                "4": {"name": "GrandChild", "parentId": "3", "url": null},
            });

            assert.deepEqual(content.metrics, {
                '5': {name: 'Time', test: '1', aggregator: null},
                '6': {name: 'Time', test: '2', aggregator: 'Total'},
                '7': {name: 'Malloc', test: '2', aggregator: 'Total'},
                '8': {name: 'Time', test: '3', aggregator: null},
                '9': {name: 'Time', test: '4', aggregator: null},
            });

            let manifest = Manifest._didFetchManifest(content);

            let someTest = Test.findById(1);
            let someTestMetric = Metric.findById(5);
            let someOtherTest = Test.findById(2);
            let someOtherTestTime = Metric.findById(6);
            let someOtherTestMalloc = Metric.findById(7);
            let childTest = Test.findById(3);
            let childTestMetric = Metric.findById(8);
            let grandChildTest = Test.findById(4);
            let ios9iphone5s = Platform.findById(23);
            let mavericks = Platform.findById(46);
            assert(someTest);
            assert(someTestMetric);
            assert(someOtherTest);
            assert(someOtherTestTime);
            assert(someOtherTestMalloc);
            assert(childTest);
            assert(childTestMetric);
            assert(grandChildTest);
            assert(ios9iphone5s);
            assert(mavericks);

            assert.equal(mavericks.name(), 'Trunk Mavericks');
            assert(mavericks.hasTest(someTest));
            assert(mavericks.hasTest(someOtherTest));
            assert(mavericks.hasTest(childTest));
            assert(mavericks.hasTest(grandChildTest));
            assert(mavericks.hasMetric(someTestMetric));
            assert(mavericks.hasMetric(someOtherTestTime));
            assert(mavericks.hasMetric(someOtherTestMalloc));
            assert(mavericks.hasMetric(childTestMetric));

            assert.equal(ios9iphone5s.name(), 'iOS 9 iPhone 5s');
            assert(ios9iphone5s.hasTest(someTest));
            assert(!ios9iphone5s.hasTest(someOtherTest));
            assert(!ios9iphone5s.hasTest(childTest));
            assert(!ios9iphone5s.hasTest(grandChildTest));
            assert(ios9iphone5s.hasMetric(someTestMetric));
            assert(!ios9iphone5s.hasMetric(someOtherTestTime));
            assert(!ios9iphone5s.hasMetric(someOtherTestMalloc));
            assert(!ios9iphone5s.hasMetric(childTestMetric));

            assert.equal(someTest.name(), 'SomeTest');
            assert.equal(someTest.parentTest(), null);
            assert.deepEqual(someTest.path(), [someTest]);
            assert(!someTest.onlyContainsSingleMetric());
            assert.deepEqual(someTest.childTests(), [childTest]);
            assert.deepEqual(someTest.metrics(), [someTestMetric]);

            assert.equal(someTestMetric.name(), 'Time');
            assert.equal(someTestMetric.aggregatorName(), null);
            assert.equal(someTestMetric.label(), 'Time');
            assert.deepEqual(someTestMetric.childMetrics(), childTest.metrics());
            assert.equal(someTestMetric.fullName(), 'SomeTest : Time');

            assert.equal(someOtherTest.name(), 'SomeOtherTest');
            assert.equal(someOtherTest.parentTest(), null);
            assert.deepEqual(someOtherTest.path(), [someOtherTest]);
            assert(!someOtherTest.onlyContainsSingleMetric());
            assert.deepEqual(someOtherTest.childTests(), []);
            assert.equal(someOtherTest.metrics().length, 2);
            assert.equal(someOtherTest.metrics()[0].name(), 'Time');
            assert.equal(someOtherTest.metrics()[0].aggregatorName(), 'Total');
            assert.equal(someOtherTest.metrics()[0].label(), 'Time : Total');
            assert.equal(someOtherTest.metrics()[0].childMetrics().length, 0);
            assert.equal(someOtherTest.metrics()[0].fullName(), 'SomeOtherTest : Time : Total');
            assert.equal(someOtherTest.metrics()[1].name(), 'Malloc');
            assert.equal(someOtherTest.metrics()[1].aggregatorName(), 'Total');
            assert.equal(someOtherTest.metrics()[1].label(), 'Malloc : Total');
            assert.equal(someOtherTest.metrics()[1].childMetrics().length, 0);
            assert.equal(someOtherTest.metrics()[1].fullName(), 'SomeOtherTest : Malloc : Total');

            assert.equal(childTest.name(), 'ChildTest');
            assert.equal(childTest.parentTest(), someTest);
            assert.deepEqual(childTest.path(), [someTest, childTest]);
            assert(!childTest.onlyContainsSingleMetric());
            assert.deepEqual(childTest.childTests(), [grandChildTest]);
            assert.equal(childTest.metrics().length, 1);
            assert.equal(childTest.metrics()[0].label(), 'Time');
            assert.equal(childTest.metrics()[0].fullName(), 'SomeTest \u220B ChildTest : Time');

            assert.equal(grandChildTest.name(), 'GrandChild');
            assert.equal(grandChildTest.parentTest(), childTest);
            assert.deepEqual(grandChildTest.path(), [someTest, childTest, grandChildTest]);
            assert(grandChildTest.onlyContainsSingleMetric());
            assert.deepEqual(grandChildTest.childTests(), []);
            assert.equal(grandChildTest.metrics().length, 1);
            assert.equal(grandChildTest.metrics()[0].label(), 'Time');
            assert.equal(grandChildTest.metrics()[0].fullName(), 'SomeTest \u220B ChildTest \u220B GrandChild : Time');
        });
    });

    it("should generate manifest with triggerables", () => {
        let db = TestServer.database();
        return Promise.all([
            db.insert('repositories', {id: 11, name: 'WebKit', url: 'https://trac.webkit.org/$1'}),
            db.insert('repositories', {id: 9, name: 'OS X'}),
            db.insert('repositories', {id: 101, name: 'WebKit', owner: 9, url: 'https://trac.webkit.org/$1'}),
            db.insert('build_triggerables', {id: 200, name: 'build.webkit.org'}),
            db.insert('build_triggerables', {id: 201, name: 'ios-build.webkit.org'}),
            db.insert('tests', {id: 1, name: 'SomeTest'}),
            db.insert('tests', {id: 2, name: 'SomeOtherTest'}),
            db.insert('tests', {id: 3, name: 'ChildTest', parent: 1}),
            db.insert('platforms', {id: 23, name: 'iOS 9 iPhone 5s'}),
            db.insert('platforms', {id: 46, name: 'Trunk Mavericks'}),
            db.insert('test_metrics', {id: 5, test: 1, name: 'Time'}),
            db.insert('test_metrics', {id: 8, test: 2, name: 'FrameRate'}),
            db.insert('test_metrics', {id: 9, test: 3, name: 'Time'}),
            db.insert('test_configurations', {id: 101, metric: 5, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 102, metric: 8, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 103, metric: 9, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 104, metric: 5, platform: 23, type: 'current'}),
            db.insert('test_configurations', {id: 105, metric: 8, platform: 23, type: 'current'}),
            db.insert('test_configurations', {id: 106, metric: 9, platform: 23, type: 'current'}),
            db.insert('triggerable_repositories', {triggerable: 200, repository: 11}),
            db.insert('triggerable_repositories', {triggerable: 201, repository: 11}),
            db.insert('triggerable_configurations', {triggerable: 200, test: 1, platform: 46}),
            db.insert('triggerable_configurations', {triggerable: 200, test: 2, platform: 46}),
            db.insert('triggerable_configurations', {triggerable: 201, test: 1, platform: 23}),
            db.insert('triggerable_configurations', {triggerable: 201, test: 2, platform: 23}),
        ]).then(() => {
            return Manifest.fetch();
        }).then(() => {
            let webkit = Repository.findById(11);
            assert.equal(webkit.name(), 'WebKit');
            assert.equal(webkit.urlForRevision(123), 'https://trac.webkit.org/123');

            let osWebkit1 = Repository.findById(101);
            assert.equal(osWebkit1.name(), 'WebKit');
            assert.equal(osWebkit1.owner(), 9);
            assert.equal(osWebkit1.urlForRevision(123), 'https://trac.webkit.org/123');

            let osx = Repository.findById(9);
            assert.equal(osx.name(), 'OS X');

            let someTest = Test.findById(1);
            assert.equal(someTest.name(), 'SomeTest');

            let someOtherTest = Test.findById(2);
            assert.equal(someOtherTest.name(), 'SomeOtherTest');

            let childTest = Test.findById(3);
            assert.equal(childTest.name(), 'ChildTest');

            let ios9iphone5s = Platform.findById(23);
            assert.equal(ios9iphone5s.name(), 'iOS 9 iPhone 5s');

            let mavericks = Platform.findById(46);
            assert.equal(mavericks.name(), 'Trunk Mavericks');

            assert.equal(Triggerable.all().length, 2);

            let osxTriggerable = Triggerable.findByTestConfiguration(someTest, mavericks);
            assert.equal(osxTriggerable.name(), 'build.webkit.org');
            assert.deepEqual(osxTriggerable.acceptedRepositories(), [webkit]);

            assert.equal(Triggerable.findByTestConfiguration(someOtherTest, mavericks), osxTriggerable);
            assert.equal(Triggerable.findByTestConfiguration(childTest, mavericks), osxTriggerable);

            let iosTriggerable = Triggerable.findByTestConfiguration(someOtherTest, ios9iphone5s);
            assert.notEqual(iosTriggerable, osxTriggerable);
            assert.equal(iosTriggerable.name(), 'ios-build.webkit.org');
            assert.deepEqual(iosTriggerable.acceptedRepositories(), [webkit]);

            assert.equal(Triggerable.findByTestConfiguration(someOtherTest, ios9iphone5s), iosTriggerable);
            assert.equal(Triggerable.findByTestConfiguration(childTest, ios9iphone5s), iosTriggerable);
        });
    });

});
