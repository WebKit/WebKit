'use strict';

const assert = require('assert');

const OSBuildFetcher = require('../tools/js/os-build-fetcher.js').OSBuildFetcher;
const MockRemoteAPI = require('../unit-tests/resources/mock-remote-api.js').MockRemoteAPI;
const TestServer = require('./resources/test-server.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;
const MockSubprocess = require('./resources/mock-subprocess.js').MockSubprocess;
const MockLogger = require('./resources/mock-logger.js').MockLogger;


describe('OSBuildFetcher', function() {
    this.timeout(5000);
    TestServer.inject('node');

    beforeEach(function () {
        MockRemoteAPI.reset('http://build.webkit.org');
        MockSubprocess.reset();
    });

    const emptyReport = {
        'slaveName': 'someSlave',
        'slavePassword': 'somePassword',
    };

    const slaveAuth = {
        'name': 'someSlave',
        'password': 'somePassword'
    };

    const ownedCommitWithWebKit = {
        'WebKit': {'revision': '141978'}
    };

    const anotherownedCommitWithWebKit = {
        'WebKit': {'revision': '141999'}
    };

    const anotherownedCommitWithWebKitAndJavaScriptCore = {
        'WebKit': {'revision': '142000'},
        'JavaScriptCore': {'revision': '142000'}
    };

    const osxCommit = {
        'repository': 'OSX',
        'revision': 'Sierra16D32',
        'order': 1603003200
    };

    const anotherOSXCommit = {
        'repository': 'OSX',
        'revision': 'Sierra16E32',
        'order': 1603003200
    };


    const config = {
        'name': 'OSX',
        'customCommands': [
            {
                'command': ['list', 'all osx 16Dxx builds'],
                'ownedCommitCommand': ['list', 'ownedCommit', 'for', 'revision'],
                'linesToIgnore': '^\\.*$',
                'minRevision': 'Sierra16D0',
                'maxRevision': 'Sierra16D999'
            },
            {
                'command': ['list', 'all osx 16Exx builds'],
                'ownedCommitCommand': ['list', 'ownedCommit', 'for', 'revision'],
                'linesToIgnore': '^\\.*$',
                'minRevision': 'Sierra16E0',
                'maxRevision': 'Sierra16E999'
            }
        ]
    };


    const configWithoutOwnedCommitCommand = {
        'name': 'OSX',
        'customCommands': [
            {
                'command': ['list', 'all osx 16Dxx builds'],
                'linesToIgnore': '^\\.*$',
                'minRevision': 'Sierra16D0',
                'maxRevision': 'Sierra16D999'
            },
            {
                'command': ['list', 'all osx 16Exx builds'],
                'linesToIgnore': '^\\.*$',
                'minRevision': 'Sierra16E0',
                'maxRevision': 'Sierra16E999'
            }
        ]
    };

    const configTrackingOneOS = {
        'name': 'OSX',
        'customCommands': [
            {
                'command': ['list', 'all osx 16Dxx builds'],
                'linesToIgnore': '^\\.*$',
                'minRevision': 'Sierra16D100',
                'maxRevision': 'Sierra16D999'
            }
        ]
    };

    describe('OSBuilderFetcher._computeOrder', () => {
        it('should calculate the right order for a given valid revision', () => {
            const fetcher = new OSBuildFetcher({});
            assert.equal(fetcher._computeOrder('Sierra16D32'), 1603003200);
            assert.equal(fetcher._computeOrder('16D321'), 1603032100);
            assert.equal(fetcher._computeOrder('16d321'), 1603032100);
            assert.equal(fetcher._computeOrder('16D321z'), 1603032126);
            assert.equal(fetcher._computeOrder('16d321Z'), 1603032126);
            assert.equal(fetcher._computeOrder('10.12.3 16D32'), 1603003200);
            assert.equal(fetcher._computeOrder('10.12.3 Sierra16D32'), 1603003200);
        });

        it('should throw assertion error when given a invalid revision', () => {
            const fetcher = new OSBuildFetcher({});
            assert.throws(() => fetcher._computeOrder('invalid'), 'AssertionError [ERR_ASSERTION]');
            assert.throws(() => fetcher._computeOrder(''), 'AssertionError [ERR_ASSERTION]');
            assert.throws(() => fetcher._computeOrder('16'), 'AssertionError [ERR_ASSERTION]');
            assert.throws(() => fetcher._computeOrder('16D'), 'AssertionError [ERR_ASSERTION]');
            assert.throws(() => fetcher._computeOrder('123'), 'AssertionError [ERR_ASSERTION]');
            assert.throws(() => fetcher._computeOrder('D123'), 'AssertionError [ERR_ASSERTION]');
            assert.throws(() => fetcher._computeOrder('123z'), 'AssertionError [ERR_ASSERTION]');
            assert.throws(() => fetcher._computeOrder('16[163'), 'AssertionError [ERR_ASSERTION]');
            assert.throws(() => fetcher._computeOrder('16D163['), 'AssertionError [ERR_ASSERTION]');
        })
    });

    describe('OSBuilderFetcher._commitsForAvailableBuilds', () => {
        it('should compatible with command output only contains lines of revision', async () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher({}, null, null, MockSubprocess, logger);
            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const fetchCommitsPromise = fetcher._commitsForAvailableBuilds(['list', 'build1'], '^\\.*$');

            await waitForInvocationPromise;
            assert.equal(MockSubprocess.invocations.length, 1);
            assert.deepEqual(MockSubprocess.invocations[0].command, ['list', 'build1']);

            const expectedResults = {allRevisions: ["16D321", "16E321z", "16F321"], commitsWithTestability: {}};
            await MockSubprocess.invocations[0].resolve('16D321\n16E321z\n\n16F321');
            const buildInfo = await fetchCommitsPromise;
            assert.deepEqual(expectedResults, buildInfo);
        });

        it('should parse the command output as JSON format', async () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher({}, null, null, MockSubprocess, logger);
            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const fetchCommitsPromise = fetcher._commitsForAvailableBuilds(['list', 'build1']);

            await waitForInvocationPromise;
            assert.equal(MockSubprocess.invocations.length, 1);
            assert.deepEqual(MockSubprocess.invocations[0].command, ['list', 'build1']);

            const outputObject = {allRevisions: ["16D321", "16E321z", "16F321"], commitsWithTestability: {"16D321": "Panic"}};
            await MockSubprocess.invocations[0].resolve(JSON.stringify(outputObject));
            const buildInfo = await fetchCommitsPromise;
            assert.deepEqual(outputObject, buildInfo);
        });
    });


    describe('OSBuilderFetcher._commitsWithinRange', () => {
        it('should only return commits whose orders are higher than specified order', async () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher({}, null, null, MockSubprocess, logger);
            const results = fetcher._commitsWithinRange(["16D321", "16E321z", "16F321"], "OSX", 1604000000, 1606000000);
            assert.equal(results.length, 2);
            assert.deepEqual(results[0], {repository: 'OSX', order: 1604032126, revision: '16E321z'});
            assert.deepEqual(results[1], {repository: 'OSX', order: 1605032100, revision: '16F321'});

        });

        it('should only return commits whose orders are higher than minOrder and lower than the maxOrder', async () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher({}, null, null, MockSubprocess, logger);
            const results = fetcher._commitsWithinRange(["16D321", "16E321z", "16F321"], "OSX", 1604000000, 1605000000);
            assert.equal(results.length, 1);
            assert.deepEqual(results[0], {repository: 'OSX', order: 1604032126, revision: '16E321z'});
        });
    });

    describe('OSBuildFetcher._addOwnedCommitsForBuild', () => {
        it('should add owned-commit info for commits', () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher({}, null, null, MockSubprocess, logger);
            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const addownedCommitPromise = fetcher._addOwnedCommitsForBuild([osxCommit, anotherOSXCommit], ['ownedCommit', 'for', 'revision']);

            return waitForInvocationPromise.then(() => {
                assert.equal(MockSubprocess.invocations.length, 1);
                assert.deepEqual(MockSubprocess.invocations[0].command, ['ownedCommit', 'for', 'revision', 'Sierra16D32']);
                MockSubprocess.invocations[0].resolve(JSON.stringify(ownedCommitWithWebKit));
                MockSubprocess.reset();
                return MockSubprocess.waitForInvocation();
            }).then(() => {
                assert.equal(MockSubprocess.invocations.length, 1);
                assert.deepEqual(MockSubprocess.invocations[0].command, ['ownedCommit', 'for', 'revision', 'Sierra16E32']);
                MockSubprocess.invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKit));
                return addownedCommitPromise;
            }).then((results) => {
                assert.equal(results.length, 2);
                assert.equal(results[0]['repository'], osxCommit['repository']);
                assert.equal(results[0]['revision'], osxCommit['revision']);
                assert.deepEqual(results[0]['ownedCommits'], ownedCommitWithWebKit);
                assert.equal(results[1]['repository'], anotherOSXCommit['repository']);
                assert.equal(results[1]['revision'], anotherOSXCommit['revision']);
                assert.deepEqual(results[1]['ownedCommits'], anotherownedCommitWithWebKit);
            });
        });

        it('should fail if the command to get owned-commit info fails', () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher({}, null, null, MockSubprocess, logger);
            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const addownedCommitPromise = fetcher._addOwnedCommitsForBuild([osxCommit], ['ownedCommit', 'for', 'revision'])

            return waitForInvocationPromise.then(() => {
                assert.equal(MockSubprocess.invocations.length, 1);
                assert.deepEqual(MockSubprocess.invocations[0].command, ['ownedCommit', 'for', 'revision', 'Sierra16D32']);
                MockSubprocess.invocations[0].reject('Failed getting owned-commit');

                return addownedCommitPromise.then(() => {
                    assert(false, 'should never be reached');
                }, (error_output) => {
                    assert(error_output);
                    assert.equal(error_output, 'Failed getting owned-commit');
                });
            });
        });


        it('should fail if entries in owned-commits does not contain revision', () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher({}, null, null, MockSubprocess, logger);
            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const addownedCommitPromise = fetcher._addOwnedCommitsForBuild([osxCommit], ['ownedCommit', 'for', 'revision'])

            return waitForInvocationPromise.then(() => {
                assert.equal(MockSubprocess.invocations.length, 1);
                assert.deepEqual(MockSubprocess.invocations[0].command, ['ownedCommit', 'for', 'revision', 'Sierra16D32']);
                MockSubprocess.invocations[0].resolve('{"WebKit":{"RandomKey": "RandomValue"}}');

                return addownedCommitPromise.then(() => {
                    assert(false, 'should never be reached');
                }, (error_output) => {
                    assert(error_output);
                    assert.equal(error_output.name, 'AssertionError [ERR_ASSERTION]');
                });
            });
        })
    });

    describe('OSBuildFetcher.fetchReportAndUpdateBuilds', () => {
        const invocations = MockSubprocess.invocations;

        beforeEach(function () {
            TestServer.database().connect({keepAlive: true});
        });

        afterEach(function () {
            TestServer.database().disconnect();
        });

        it('should be backward compatible and report all build commits with owned-commits', () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher(config, TestServer.remoteAPI(), slaveAuth, MockSubprocess, logger);
            const db = TestServer.database();

            let fetchReportAndUpdateBuildsPromise = null;
            let fetchAvailableBuildsPromise = null;

            return addSlaveForReport(emptyReport).then(() => {
                return Promise.all([
                    db.insert('repositories', {'id': 10, 'name': 'OSX'}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D67', 'order': 1603006700, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D68', 'order': 1603006800, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D69', 'order': 1603006900, 'reported': false}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E32', 'order': 1604003200, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33', 'order': 1604003300, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33g', 'order': 1604003307, 'reported': true})]);
            }).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16D68');
                assert.equal(result['commits'][0]['order'], 1603006800);
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16E33g');
                assert.equal(result['commits'][0]['order'], 1604003307);
                const waitForInvocationPromise = MockSubprocess.waitForInvocation();
                fetchAvailableBuildsPromise = fetcher._fetchAvailableBuilds();
                return waitForInvocationPromise;
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
                invocations[0].resolve('\n\nSierra16D68\nSierra16D69\n');
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16D69']);
                invocations[0].resolve(JSON.stringify(ownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Exx builds']);
                invocations[0].resolve('\n\nSierra16E32\nSierra16E33\nSierra16E33h\nSierra16E34');
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E33h']);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E34']);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKitAndJavaScriptCore));
                return fetchAvailableBuildsPromise;
            }).then(() => {
                MockSubprocess.reset();
                fetchReportAndUpdateBuildsPromise = fetcher.fetchReportAndUpdateBuilds();
                return MockSubprocess.waitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
                invocations[0].resolve('\n\nSierra16D68\nSierra16D69\n');
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16D69']);
                invocations[0].resolve(JSON.stringify(ownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Exx builds']);
                invocations[0].resolve('\n\nSierra16E32\nSierra16E33\nSierra16E33h\nSierra16E34');
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E33h']);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKitAndJavaScriptCore));
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E34']);

                return fetchReportAndUpdateBuildsPromise;
            }).then(() => {
                return Promise.all([
                    db.selectRows('repositories', {'name': 'WebKit'}),
                    db.selectRows('repositories', {'name': 'JavaScriptCore'}),
                    db.selectRows('commits', {'revision': 'Sierra16D69'}),
                    db.selectRows('commits', {'revision': 'Sierra16E33h'}),
                    db.selectRows('commits', {'revision': 'Sierra16E34'})]);
            }).then((results) => {
                const webkitRepository = results[0];
                const jscRepository = results[1];
                const osxCommit16D69 = results[2];
                const osxCommit16E33h = results[3];
                const osxCommit16E34 = results[4];

                assert.equal(webkitRepository.length, 1);
                assert.equal(webkitRepository[0]['owner'], 10);
                assert.equal(jscRepository.length, 1);
                assert.equal(jscRepository[0]['owner'], 10);

                assert.equal(osxCommit16D69.length, 1);
                assert.equal(osxCommit16D69[0]['repository'], 10);
                assert.equal(osxCommit16D69[0]['order'], 1603006900);

                assert.equal(osxCommit16E33h.length, 1);
                assert.equal(osxCommit16E33h[0]['repository'], 10);
                assert.equal(osxCommit16E33h[0]['order'], 1604003308);

                assert.equal(osxCommit16E34.length, 1);
                assert.equal(osxCommit16E34[0]['repository'], 10);
                assert.equal(osxCommit16E34[0]['order'], 1604003400);

                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16D69');
                assert.equal(result['commits'][0]['order'], 1603006900);

                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16E34');
                assert.equal(result['commits'][0]['order'], 1604003400);
            });
        });

        it('should report all build commits with owned-commits', () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher(config, TestServer.remoteAPI(), slaveAuth, MockSubprocess, logger);
            const db = TestServer.database();
            const resultsForSierraD = {allRevisions: ["Sierra16D68", "Sierra16D69"], commitsWithTestability: {}};
            const resultsForSierraE = {allRevisions: ["Sierra16E32", "Sierra16E33", "Sierra16E33h", "Sierra16E34"], commitsWithTestability: {}};

            let fetchReportAndUpdateBuildsPromise = null;
            let fetchAvailableBuildsPromise = null;

            return addSlaveForReport(emptyReport).then(() => {
                return Promise.all([
                    db.insert('repositories', {'id': 10, 'name': 'OSX'}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D67', 'order': 1603006700, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D68', 'order': 1603006800, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D69', 'order': 1603006900, 'reported': false}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E32', 'order': 1604003200, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33', 'order': 1604003300, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33g', 'order': 1604003307, 'reported': true})]);
            }).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16D68');
                assert.equal(result['commits'][0]['order'], 1603006800);
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16E33g');
                assert.equal(result['commits'][0]['order'], 1604003307);
                const waitForInvocationPromise = MockSubprocess.waitForInvocation();
                fetchAvailableBuildsPromise = fetcher._fetchAvailableBuilds();
                return waitForInvocationPromise;
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
                invocations[0].resolve(JSON.stringify(resultsForSierraD));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16D69']);
                invocations[0].resolve(JSON.stringify(ownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Exx builds']);
                invocations[0].resolve(JSON.stringify(resultsForSierraE));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E33h']);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E34']);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKitAndJavaScriptCore));
                return fetchAvailableBuildsPromise;
            }).then(() => {
                MockSubprocess.reset();
                fetchReportAndUpdateBuildsPromise = fetcher.fetchReportAndUpdateBuilds();
                return MockSubprocess.waitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
                invocations[0].resolve(JSON.stringify(resultsForSierraD));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16D69']);
                invocations[0].resolve(JSON.stringify(ownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Exx builds']);
                invocations[0].resolve(JSON.stringify(resultsForSierraE));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E33h']);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKitAndJavaScriptCore));
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E34']);

                return fetchReportAndUpdateBuildsPromise;
            }).then(() => {
                return Promise.all([
                    db.selectRows('repositories', {'name': 'WebKit'}),
                    db.selectRows('repositories', {'name': 'JavaScriptCore'}),
                    db.selectRows('commits', {'revision': 'Sierra16D69'}),
                    db.selectRows('commits', {'revision': 'Sierra16E33h'}),
                    db.selectRows('commits', {'revision': 'Sierra16E34'})]);
            }).then((results) => {
                const webkitRepository = results[0];
                const jscRepository = results[1];
                const osxCommit16D69 = results[2];
                const osxCommit16E33h = results[3];
                const osxCommit16E34 = results[4];

                assert.equal(webkitRepository.length, 1);
                assert.equal(webkitRepository[0]['owner'], 10);
                assert.equal(jscRepository.length, 1);
                assert.equal(jscRepository[0]['owner'], 10);

                assert.equal(osxCommit16D69.length, 1);
                assert.equal(osxCommit16D69[0]['repository'], 10);
                assert.equal(osxCommit16D69[0]['order'], 1603006900);

                assert.equal(osxCommit16E33h.length, 1);
                assert.equal(osxCommit16E33h[0]['repository'], 10);
                assert.equal(osxCommit16E33h[0]['order'], 1604003308);

                assert.equal(osxCommit16E34.length, 1);
                assert.equal(osxCommit16E34[0]['repository'], 10);
                assert.equal(osxCommit16E34[0]['order'], 1604003400);

                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16D69');
                assert.equal(result['commits'][0]['order'], 1603006900);

                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16E34');
                assert.equal(result['commits'][0]['order'], 1604003400);
            });
        });

        it('should update testability warning for commits', async () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher(config, TestServer.remoteAPI(), slaveAuth, MockSubprocess, logger);
            const db = TestServer.database();
            const resultsForSierraD = {allRevisions: ["Sierra16D68", "Sierra16D69"], commitsWithTestability: {"Sierra16D68": "Panic", "Sierra16D69": "Spin CPU"}};
            const resultsForSierraE = {allRevisions: ["Sierra16E32", "Sierra16E33", "Sierra16E33h", "Sierra16E34"], commitsWithTestability: {"Sierra16E31": "WebKit crashes"}};

            await addSlaveForReport(emptyReport);

            await Promise.all([
                db.insert('repositories', {'id': 10, 'name': 'OSX'}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D67', 'order': 1603006700, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D68', 'order': 1603006800, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D69', 'order': 1603006900, 'reported': false}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E31', 'order': 1604003100, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E32', 'order': 1604003200, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33', 'order': 1604003300, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33g', 'order': 1604003307, 'reported': true})]);

            let result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');

            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D68');
            assert.equal(result['commits'][0]['order'], 1603006800);
            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');

            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16E33g');
            assert.equal(result['commits'][0]['order'], 1604003307);

            const fetchReportAndUpdatePromise = fetcher.fetchReportAndUpdateBuilds();
            await MockSubprocess.waitForInvocation();

            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
            invocations[0].resolve(JSON.stringify(resultsForSierraD));
            await MockSubprocess.resetAndWaitForInvocation();

            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16D69']);
            invocations[0].resolve(JSON.stringify(ownedCommitWithWebKit));
            await MockSubprocess.resetAndWaitForInvocation();

            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'all osx 16Exx builds']);
            invocations[0].resolve(JSON.stringify(resultsForSierraE));

            await MockSubprocess.resetAndWaitForInvocation();

            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E33h']);
            invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKit));
            await  MockSubprocess.resetAndWaitForInvocation();
            assert.equal(invocations.length, 1);
            invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKitAndJavaScriptCore));
            assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E34']);

            await fetchReportAndUpdatePromise;

            const webkitRepository = await db.selectRows('repositories', {'name': 'WebKit'});
            const jscRepository = await db.selectRows('repositories', {'name': 'JavaScriptCore'});
            const osxCommit16D68 = await db.selectRows('commits', {'revision': 'Sierra16D68'});
            const osxCommit16D69 = await db.selectRows('commits', {'revision': 'Sierra16D69'});
            const osxCommit16E31 = await db.selectRows('commits', {'revision': 'Sierra16E31'});
            const osxCommit16E33h = await db.selectRows('commits', {'revision': 'Sierra16E33h'});
            const osxCommit16E34 = await db.selectRows('commits', {'revision': 'Sierra16E34'});

            assert.equal(webkitRepository.length, 1);
            assert.equal(webkitRepository[0]['owner'], 10);
            assert.equal(jscRepository.length, 1);
            assert.equal(jscRepository[0]['owner'], 10);

            assert.equal(osxCommit16D68.length, 1);
            assert.equal(osxCommit16D68[0]['repository'], 10);
            assert.equal(osxCommit16D68[0]['order'], 1603006800);
            assert.equal(osxCommit16D68[0]['testability'], "Panic");

            assert.equal(osxCommit16D69.length, 1);
            assert.equal(osxCommit16D69[0]['repository'], 10);
            assert.equal(osxCommit16D69[0]['order'], 1603006900);
            assert.equal(osxCommit16D69[0]['testability'], "Spin CPU");

            assert.equal(osxCommit16E31.length, 1);
            assert.equal(osxCommit16E31[0]['repository'], 10);
            assert.equal(osxCommit16E31[0]['order'], 1604003100);
            assert.equal(osxCommit16E31[0]['testability'], "WebKit crashes");

            assert.equal(osxCommit16E33h.length, 1);
            assert.equal(osxCommit16E33h[0]['repository'], 10);
            assert.equal(osxCommit16E33h[0]['order'], 1604003308);

            assert.equal(osxCommit16E34.length, 1);
            assert.equal(osxCommit16E34[0]['repository'], 10);
            assert.equal(osxCommit16E34[0]['order'], 1604003400);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D69');
            assert.equal(result['commits'][0]['order'], 1603006900);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16E34');
            assert.equal(result['commits'][0]['order'], 1604003400);
        });

        it('should report commits without owned-commits if "ownedCommitCommand" is not specified in config', async () => {

            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher(configWithoutOwnedCommitCommand, TestServer.remoteAPI(), slaveAuth, MockSubprocess, logger);
            const db = TestServer.database();
            const resultsForSierraD = {allRevisions: ["Sierra16D68", "Sierra16D69"], commitsWithTestability: {}};
            const resultsForSierraE = {allRevisions: ["Sierra16E32", "Sierra16E33", "Sierra16E33h", "Sierra16E34"], commitsWithTestability: {}};

            await addSlaveForReport(emptyReport);
            await Promise.all([
                db.insert('repositories', {'id': 10, 'name': 'OSX'}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D67', 'order': 1603006700, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D68', 'order': 1603006800, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D69', 'order': 1603006900, 'reported': false}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E32', 'order': 1604003200, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33', 'order': 1604003300, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33g', 'order': 1604003307, 'reported': true})]);

            let result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D68');
            assert.equal(result['commits'][0]['order'], 1603006800);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16E33g');
            assert.equal(result['commits'][0]['order'], 1604003307);

            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const fetchReportAndUpdatePromise = fetcher.fetchReportAndUpdateBuilds();
            await waitForInvocationPromise;
            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
            invocations[0].resolve(JSON.stringify(resultsForSierraD));

            await MockSubprocess.resetAndWaitForInvocation();
            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'all osx 16Exx builds']);
            invocations[0].resolve(JSON.stringify(resultsForSierraE));

            result = await fetchReportAndUpdatePromise;
            const results = await Promise.all([
                db.selectRows('repositories', {'name': 'WebKit'}),
                db.selectRows('repositories', {'name': 'JavaScriptCore'}),
                db.selectRows('commits', {'revision': 'Sierra16D69'}),
                db.selectRows('commits', {'revision': 'Sierra16E33h'}),
                db.selectRows('commits', {'revision': 'Sierra16E34'})]);

            const webkitRepository = results[0];
            const jscRepository = results[1];
            const osxCommit16D69 = results[2];
            const osxCommit16E33h = results[3];
            const osxCommit16E34 = results[4];

            assert.equal(webkitRepository.length, 0);
            assert.equal(jscRepository.length, 0);

            assert.equal(osxCommit16D69.length, 1);
            assert.equal(osxCommit16D69[0]['repository'], 10);
            assert.equal(osxCommit16D69[0]['order'], 1603006900);

            assert.equal(osxCommit16E33h.length, 1);
            assert.equal(osxCommit16E33h[0]['repository'], 10);
            assert.equal(osxCommit16E33h[0]['order'], 1604003308);

            assert.equal(osxCommit16E34.length, 1);
            assert.equal(osxCommit16E34[0]['repository'], 10);
            assert.equal(osxCommit16E34[0]['order'], 1604003400);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D69');
            assert.equal(result['commits'][0]['order'], 1603006900);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16E34');
            assert.equal(result['commits'][0]['order'], 1604003400);
        });

        it('should report commits within specified revision range', async () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher(configWithoutOwnedCommitCommand, TestServer.remoteAPI(), slaveAuth, MockSubprocess, logger);
            const db = TestServer.database();
            const resultsForSierraD = {allRevisions: ["Sierra16D68", "Sierra16D69", "Sierra16D10000"], commitsWithTestability: {}};
            const resultsForSierraE = {allRevisions: ["Sierra16E32", "Sierra16E33", "Sierra16E33h", "Sierra16E34", "Sierra16E10000"], commitsWithTestability: {}};

            await addSlaveForReport(emptyReport);
            await Promise.all([
                db.insert('repositories', {'id': 10, 'name': 'OSX'}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D67', 'order': 1603006700, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D68', 'order': 1603006800, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16D69', 'order': 1603006900, 'reported': false}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E32', 'order': 1604003200, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33', 'order': 1604003300, 'reported': true}),
                db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33g', 'order': 1604003307, 'reported': true})]);

            let result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D68');
            assert.equal(result['commits'][0]['order'], 1603006800);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16E33g');
            assert.equal(result['commits'][0]['order'], 1604003307);
            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const fetchReportAndUpdatePromise = fetcher.fetchReportAndUpdateBuilds();

            await waitForInvocationPromise;
            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
            invocations[0].resolve(JSON.stringify(resultsForSierraD));

            await MockSubprocess.resetAndWaitForInvocation();
            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'all osx 16Exx builds']);
            invocations[0].resolve(JSON.stringify(resultsForSierraE));

            result = await fetchReportAndUpdatePromise;
            const results = await Promise.all([
                db.selectRows('repositories', {'name': 'WebKit'}),
                db.selectRows('repositories', {'name': 'JavaScriptCore'}),
                db.selectRows('commits', {'revision': 'Sierra16D69'}),
                db.selectRows('commits', {'revision': 'Sierra16E33h'}),
                db.selectRows('commits', {'revision': 'Sierra16E34'})]);

            const webkitRepository = results[0];
            const jscRepository = results[1];
            const osxCommit16D69 = results[2];
            const osxCommit16E33h = results[3];
            const osxCommit16E34 = results[4];

            assert.equal(webkitRepository.length, 0);
            assert.equal(jscRepository.length, 0);

            assert.equal(osxCommit16D69.length, 1);
            assert.equal(osxCommit16D69[0]['repository'], 10);
            assert.equal(osxCommit16D69[0]['order'], 1603006900);

            assert.equal(osxCommit16E33h.length, 1);
            assert.equal(osxCommit16E33h[0]['repository'], 10);
            assert.equal(osxCommit16E33h[0]['order'], 1604003308);

            assert.equal(osxCommit16E34.length, 1);
            assert.equal(osxCommit16E34[0]['repository'], 10);
            assert.equal(osxCommit16E34[0]['order'], 1604003400);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D69');
            assert.equal(result['commits'][0]['order'], 1603006900);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16E34');
            assert.equal(result['commits'][0]['order'], 1604003400);
        });

        it('should use "last-reported" order + 1 as "minOrder"', async () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher(configTrackingOneOS, TestServer.remoteAPI(), slaveAuth, MockSubprocess, logger);
            const db = TestServer.database();
            const resultsForSierraD = {allRevisions: ["Sierra16D68", "Sierra16D69", "Sierra16D100", "Sierra16D100a"], commitsWithTestability: {}};

            await addSlaveForReport(emptyReport);
            await db.insert('repositories', {'id': 10, 'name': 'OSX'});
            await db.insert('commits', {'repository': 10, 'revision': 'Sierra16D100', 'order': 1603010000, 'reported': true});

            let result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603010000&to=1603099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D100');
            assert.equal(result['commits'][0]['order'], 1603010000);

            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const fetchAndReportPromise = fetcher.fetchReportAndUpdateBuilds();
            await waitForInvocationPromise;
            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
            invocations[0].resolve(JSON.stringify(resultsForSierraD));

            result = await fetchAndReportPromise;
            const results = await Promise.all([
                db.selectRows('repositories', {'name': 'WebKit'}),
                db.selectRows('repositories', {'name': 'JavaScriptCore'}),
                db.selectRows('commits', {'revision': 'Sierra16D69'}),
                db.selectRows('commits', {'revision': 'Sierra16D100a'})]);

            const webkitRepository = results[0];
            const jscRepository = results[1];
            const osxCommit16D69 = results[2];
            const osxCommit16D100a = results[3];

            assert.equal(webkitRepository.length, 0);
            assert.equal(jscRepository.length, 0);

            assert.equal(osxCommit16D69.length, 0);

            assert.equal(osxCommit16D100a.length, 1);
            assert.equal(osxCommit16D100a[0]['repository'], 10);
            assert.equal(osxCommit16D100a[0]['order'], 1603010001);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603010000&to=1603099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D100a');
            assert.equal(result['commits'][0]['order'], 1603010001);
        });

        it('should use minRevision in the config if there is no commit', async () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher(configTrackingOneOS, TestServer.remoteAPI(), slaveAuth, MockSubprocess, logger);
            const db = TestServer.database();
            const resultsForSierraD = {allRevisions: ["Sierra16D68", "Sierra16D69", "Sierra16D100", "Sierra16D101"], commitsWithTestability: {}};

            await addSlaveForReport(emptyReport);
            await db.insert('repositories', {'id': 10, 'name': 'OSX'});

            let result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603010000&to=1603099900');
            assert.equal(result['commits'].length, 0);

            const waitForInvocationPromise = MockSubprocess.waitForInvocation();
            const fetchReportAndUpdatePromise = fetcher.fetchReportAndUpdateBuilds();
            await waitForInvocationPromise;
            assert.equal(invocations.length, 1);
            assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
            invocations[0].resolve(JSON.stringify(resultsForSierraD));

            result = await fetchReportAndUpdatePromise;
            const results = await Promise.all([
                db.selectRows('repositories', {'name': 'WebKit'}),
                db.selectRows('repositories', {'name': 'JavaScriptCore'}),
                db.selectRows('commits', {'revision': 'Sierra16D69'}),
                db.selectRows('commits', {'revision': 'Sierra16D100'}),
                db.selectRows('commits', {'revision': 'Sierra16D101'})]);

            const webkitRepository = results[0];
            const jscRepository = results[1];
            const osxCommit16D69 = results[2];
            const osxCommit16D100 = results[3];
            const osxCommit16D101 = results[4];

            assert.equal(webkitRepository.length, 0);
            assert.equal(jscRepository.length, 0);

            assert.equal(osxCommit16D69.length, 0);

            assert.equal(osxCommit16D100.length, 1);
            assert.equal(osxCommit16D100[0]['repository'], 10);
            assert.equal(osxCommit16D100[0]['order'], 1603010000);

            assert.equal(osxCommit16D101.length, 1);
            assert.equal(osxCommit16D101[0]['repository'], 10);
            assert.equal(osxCommit16D101[0]['order'], 1603010100);

            result = await TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603010000&to=1603099900');
            assert.equal(result['commits'].length, 1);
            assert.equal(result['commits'][0]['revision'], 'Sierra16D101');
            assert.equal(result['commits'][0]['order'], 1603010100);
        });

        it('should stop reporting if any custom command fails', () => {
            const logger = new MockLogger;
            const fetcher = new OSBuildFetcher(config, TestServer.remoteAPI(), slaveAuth, MockSubprocess, logger);
            const db = TestServer.database();
            const resultsForSierraD = {allRevisions: ["Sierra16D68", "Sierra16D69"], commitsWithTestability: {}};
            const resultsForSierraE = {allRevisions: ["Sierra16E32", "Sierra16E33", "Sierra16E33h", "Sierra16E34"], commitsWithTestability: {}};
            let fetchAndReportPromise = null;

            return addSlaveForReport(emptyReport).then(() => {
                return Promise.all([
                    db.insert('repositories', {'id': 10, 'name': 'OSX'}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D67', 'order': 1603006700, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D68', 'order': 1603006800, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16D69', 'order': 1603006900, 'reported': false}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E32', 'order': 1604003200, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33', 'order': 1604003300, 'reported': true}),
                    db.insert('commits', {'repository': 10, 'revision': 'Sierra16E33g', 'order': 1604003307, 'reported': true})]);
            }).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16D68');
                assert.equal(result['commits'][0]['order'], 1603006800);

                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16E33g');
                assert.equal(result['commits'][0]['order'], 1604003307);

                const waitForInvocationPromise = MockSubprocess.waitForInvocation();
                fetchAndReportPromise = fetcher.fetchReportAndUpdateBuilds();
                return waitForInvocationPromise;
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Dxx builds']);
                invocations[0].resolve(JSON.stringify(resultsForSierraD));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16D69']);
                MockSubprocess.invocations[0].resolve(JSON.stringify(ownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'all osx 16Exx builds']);
                invocations[0].resolve(JSON.stringify(resultsForSierraE));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E33h']);
                invocations[0].resolve(JSON.stringify(anotherownedCommitWithWebKit));
                return MockSubprocess.resetAndWaitForInvocation();
            }).then(() => {
                assert.equal(invocations.length, 1);
                assert.deepEqual(invocations[0].command, ['list', 'ownedCommit', 'for', 'revision', 'Sierra16E34']);
                invocations[0].reject('Command failed');
                return fetchAndReportPromise.then(() => {
                    assert(false, 'should never be reached');
                }, (error_output) => {
                    assert(error_output);
                    assert.equal(error_output, 'Command failed');
                });
            }).then(() => {
                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1603000000&to=1603099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16D68');
                assert.equal(result['commits'][0]['order'], 1603006800);

                return TestServer.remoteAPI().getJSON('/api/commits/OSX/last-reported?from=1604000000&to=1604099900');
            }).then((result) => {
                assert.equal(result['commits'].length, 1);
                assert.equal(result['commits'][0]['revision'], 'Sierra16E33g');
                assert.equal(result['commits'][0]['order'], 1604003307);
            });
        })
    })
});
