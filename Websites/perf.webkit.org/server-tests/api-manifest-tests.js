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
                'fileUploadSizeLimit', 'metrics', 'repositories', 'siteTitle', 'status', 'summaryPages', 'testAgeToleranceInHours', 'tests', 'triggerables']);

            assert.deepStrictEqual(manifest, {
                siteTitle: TestServer.testConfig().siteTitle,
                all: {},
                bugTrackers: {},
                builders: {},
                dashboard: {},
                dashboards: {},
                fileUploadSizeLimit: 2097152, // 2MB during testing.
                metrics: {},
                repositories: {},
                testAgeToleranceInHours: null,
                tests: {},
                triggerables: {},
                summaryPages: [],
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

    it("should clear Bug and BugTracker static maps when reset", async () => {
        await TestServer.database().insert('bug_trackers', bugzillaData);
        const content = await TestServer.remoteAPI().getJSON('/api/manifest');
        assert.deepEqual(content.bugTrackers, {1: {name: 'Bugzilla', bugUrl: 'https://webkit.org/b/$number',
            newBugUrl: 'https://bugs.webkit.org/', repositories: null}});

        Manifest._didFetchManifest(content);
        const trackerFromFirstFetch = BugTracker.findById(1);

        Manifest.reset();
        assert(!BugTracker.findById(1));

        Manifest._didFetchManifest(content);
        const trackerFromSecondFetch = BugTracker.findById(1);
        assert(trackerFromFirstFetch != trackerFromSecondFetch);
    });

    it("should generate manifest with bug trackers and repositories", () => {
        let db = TestServer.database();
        return Promise.all([
            db.insert('bug_trackers', bugzillaData),
            db.insert('bug_trackers', radarData),
            db.insert('repositories', {id: 11, name: 'WebKit', url: 'https://trac.webkit.org/$1'}),
            db.insert('repositories', {id: 9, name: 'macOS'}),
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

            let macos = Repository.findById(9);
            assert(macos);
            assert.equal(macos.name(), 'macOS');

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
            assert.deepEqual(Repository.sortByName(tracker.repositories()), [ios, macos]);
        });
    });

    it("should generate manifest with repositories and each repository should know its owned repositories", () => {
        const db = TestServer.database();
        return Promise.all([
            db.insert('repositories', {id: 11, name: 'WebKit', url: 'https://trac.webkit.org/$1'}),
            db.insert('repositories', {id: 9, name: 'OS X'}),
            db.insert('repositories', {id: 22, name: 'iOS'}),
            db.insert('repositories', {id: 35, name: 'JavaScriptCore', owner: 9}),
        ]).then(() => {
            return TestServer.remoteAPI().getJSON('/api/manifest');
        }).then((content) => {
            let manifest = Manifest._didFetchManifest(content);

            let webkit = Repository.findById(11);
            assert(webkit);
            assert.equal(webkit.name(), 'WebKit');
            assert.equal(webkit.urlForRevision(123), 'https://trac.webkit.org/123');
            assert.equal(webkit.ownedRepositories(), null);

            let osx = Repository.findById(9);
            assert(osx);
            assert.equal(osx.name(), 'OS X');
            assert.deepEqual(osx.ownedRepositories(), [Repository.findById(35)]);

            let ios = Repository.findById(22);
            assert(ios);
            assert.equal(ios.name(), 'iOS');
            assert.equal(ios.ownedRepositories(), null);
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
            db.insert('repositories', {id: 9, name: 'macOS'}),
            db.insert('repositories', {id: 16, name: 'Shared'}),
            db.insert('repositories', {id: 101, name: 'WebKit', owner: 9, url: 'https://trac.webkit.org/$1'}),
            db.insert('build_triggerables', {id: 200, name: 'build.webkit.org'}),
            db.insert('build_triggerables', {id: 201, name: 'ios-build.webkit.org'}),
            db.insert('build_triggerables', {id: 202, name: 'mac-build.webkit.org', disabled: true}),
            db.insert('tests', {id: 1, name: 'SomeTest'}),
            db.insert('tests', {id: 2, name: 'SomeOtherTest'}),
            db.insert('tests', {id: 3, name: 'ChildTest', parent: 1}),
            db.insert('platforms', {id: 23, name: 'iOS 9 iPhone 5s'}),
            db.insert('platforms', {id: 46, name: 'Trunk Mavericks'}),
            db.insert('platforms', {id: 104, name: 'Trunk Sierra MacBookPro11,2'}),
            db.insert('test_metrics', {id: 5, test: 1, name: 'Time'}),
            db.insert('test_metrics', {id: 8, test: 2, name: 'FrameRate'}),
            db.insert('test_metrics', {id: 9, test: 3, name: 'Time'}),
            db.insert('test_configurations', {id: 101, metric: 5, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 102, metric: 8, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 103, metric: 9, platform: 46, type: 'current'}),
            db.insert('test_configurations', {id: 104, metric: 5, platform: 23, type: 'current'}),
            db.insert('test_configurations', {id: 105, metric: 8, platform: 23, type: 'current'}),
            db.insert('test_configurations', {id: 106, metric: 9, platform: 23, type: 'current'}),
            db.insert('test_configurations', {id: 107, metric: 5, platform: 104, type: 'current'}),
            db.insert('test_configurations', {id: 108, metric: 8, platform: 104, type: 'current'}),
            db.insert('test_configurations', {id: 109, metric: 9, platform: 104, type: 'current'}),
            db.insert('triggerable_repository_groups', {id: 300, triggerable: 200, name: 'default'}),
            db.insert('triggerable_repository_groups', {id: 301, triggerable: 201, name: 'default'}),
            db.insert('triggerable_repository_groups', {id: 302, triggerable: 202, name: 'system-and-webkit'}),
            db.insert('triggerable_repository_groups', {id: 303, triggerable: 202, name: 'system-and-roots', accepts_roots: true}),
            db.insert('triggerable_repository_groups', {id: 304, triggerable: 202, name: 'webkit-and-shared', hidden: true}),
            db.insert('triggerable_repositories', {group: 300, repository: 11}),
            db.insert('triggerable_repositories', {group: 301, repository: 11}),
            db.insert('triggerable_repositories', {group: 302, repository: 11}),
            db.insert('triggerable_repositories', {group: 304, repository: 11}),
            db.insert('triggerable_repositories', {group: 302, repository: 9}),
            db.insert('triggerable_repositories', {group: 303, repository: 9}),
            db.insert('triggerable_repositories', {group: 304, repository: 16}),
            db.insert('triggerable_configurations', {triggerable: 200, test: 1, platform: 46}),
            db.insert('triggerable_configurations', {triggerable: 200, test: 2, platform: 46}),
            db.insert('triggerable_configurations', {triggerable: 201, test: 1, platform: 23}),
            db.insert('triggerable_configurations', {triggerable: 201, test: 2, platform: 23}),
            db.insert('triggerable_configurations', {triggerable: 202, test: 1, platform: 104}),
            db.insert('triggerable_configurations', {triggerable: 202, test: 2, platform: 104}),
        ]).then(() => {
            return Manifest.fetch();
        }).then(() => {
            const webkit = Repository.findById(11);
            assert.equal(webkit.name(), 'WebKit');
            assert.equal(webkit.urlForRevision(123), 'https://trac.webkit.org/123');

            const osWebkit1 = Repository.findById(101);
            assert.equal(osWebkit1.name(), 'WebKit');
            assert.equal(osWebkit1.ownerId(), 9);
            assert.equal(osWebkit1.urlForRevision(123), 'https://trac.webkit.org/123');

            const macos = Repository.findById(9);
            assert.equal(macos.name(), 'macOS');

            const shared = Repository.findById(16);
            assert.equal(shared.name(), 'Shared');

            const someTest = Test.findById(1);
            assert.equal(someTest.name(), 'SomeTest');

            const someOtherTest = Test.findById(2);
            assert.equal(someOtherTest.name(), 'SomeOtherTest');

            const childTest = Test.findById(3);
            assert.equal(childTest.name(), 'ChildTest');

            const ios9iphone5s = Platform.findById(23);
            assert.equal(ios9iphone5s.name(), 'iOS 9 iPhone 5s');

            const mavericks = Platform.findById(46);
            assert.equal(mavericks.name(), 'Trunk Mavericks');

            const sierra = Platform.findById(104);
            assert.equal(sierra.name(), 'Trunk Sierra MacBookPro11,2');

            assert.equal(Triggerable.all().length, 3);

            const macosTriggerable = Triggerable.findByTestConfiguration(someTest, mavericks);
            assert.equal(macosTriggerable.name(), 'build.webkit.org');

            assert.equal(Triggerable.findByTestConfiguration(someOtherTest, mavericks), macosTriggerable);
            assert.equal(Triggerable.findByTestConfiguration(childTest, mavericks), macosTriggerable);

            const iosTriggerable = Triggerable.findByTestConfiguration(someOtherTest, ios9iphone5s);
            assert.notEqual(iosTriggerable, macosTriggerable);
            assert.equal(iosTriggerable.name(), 'ios-build.webkit.org');

            assert.equal(Triggerable.findByTestConfiguration(someOtherTest, ios9iphone5s), iosTriggerable);
            assert.equal(Triggerable.findByTestConfiguration(childTest, ios9iphone5s), iosTriggerable);

            const macTriggerable = Triggerable.findByTestConfiguration(someTest, sierra);
            assert.equal(macTriggerable.name(), 'mac-build.webkit.org');
            assert(macTriggerable.acceptedTests().has(someTest));

            const groups = macTriggerable.repositoryGroups();
            assert.deepEqual(groups.length, 3);
            TriggerableRepositoryGroup.sortByName(groups);

            const emptyCustomSet = new CustomCommitSet;

            const customSetWithOSX = new CustomCommitSet;
            customSetWithOSX.setRevisionForRepository(macos, '10.11 15A284');

            const cusomSetWithOSXAndWebKit = new CustomCommitSet;
            cusomSetWithOSXAndWebKit.setRevisionForRepository(webkit, '191622');
            cusomSetWithOSXAndWebKit.setRevisionForRepository(macos, '10.11 15A284');

            const cusomSetWithWebKit = new CustomCommitSet;
            cusomSetWithWebKit.setRevisionForRepository(webkit, '191622');

            const cusomSetWithWebKitAndShared = new CustomCommitSet;
            cusomSetWithWebKitAndShared.setRevisionForRepository(webkit, '191622');
            cusomSetWithWebKitAndShared.setRevisionForRepository(shared, '86456');

            assert.equal(groups[0].name(), 'system-and-roots');
            assert.equal(groups[0].isHidden(), false);
            assert.equal(groups[0].acceptsCustomRoots(), true);
            assert.deepEqual(Repository.sortByName(groups[0].repositories()), [macos]);
            assert.equal(groups[0].accepts(emptyCustomSet), false);
            assert.equal(groups[0].accepts(customSetWithOSX), true);
            assert.equal(groups[0].accepts(cusomSetWithOSXAndWebKit), false);
            assert.equal(groups[0].accepts(cusomSetWithWebKitAndShared), false);
            assert.equal(groups[0].accepts(cusomSetWithWebKit), false);

            assert.equal(groups[1].name(), 'system-and-webkit');
            assert.equal(groups[1].isHidden(), false);
            assert.equal(groups[1].acceptsCustomRoots(), false);
            assert.deepEqual(Repository.sortByName(groups[1].repositories()), [webkit, macos]);
            assert.equal(groups[1].accepts(emptyCustomSet), false);
            assert.equal(groups[1].accepts(customSetWithOSX), false);
            assert.equal(groups[1].accepts(cusomSetWithOSXAndWebKit), true);
            assert.equal(groups[1].accepts(cusomSetWithWebKitAndShared), false);
            assert.equal(groups[1].accepts(cusomSetWithWebKit), false);

            assert.equal(groups[2].name(), 'webkit-and-shared');
            assert.equal(groups[2].isHidden(), true);
            assert.equal(groups[2].acceptsCustomRoots(), false);
            assert.deepEqual(Repository.sortByName(groups[2].repositories()), [shared, webkit]);
            assert.equal(groups[2].accepts(emptyCustomSet), false);
            assert.equal(groups[2].accepts(customSetWithOSX), false);
            assert.equal(groups[2].accepts(cusomSetWithOSXAndWebKit), false);
            assert.equal(groups[2].accepts(cusomSetWithWebKitAndShared), true);
            assert.equal(groups[2].accepts(cusomSetWithWebKit), false);
        });
    });

});
