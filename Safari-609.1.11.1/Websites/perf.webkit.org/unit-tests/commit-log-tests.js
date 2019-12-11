'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');
const BrowserPrivilegedAPI = require('../public/v3/privileged-api.js').PrivilegedAPI;
const MockModels = require('./resources/mock-v3-models.js').MockModels;
const MockRemoteAPI = require('../unit-tests/resources/mock-remote-api.js').MockRemoteAPI;

function webkitCommit()
{
    return new CommitLog(1, {
        id: 1,
        repository: MockModels.webkit,
        revision: '200805',
        time: +(new Date('2016-05-13T00:55:57.841344Z')),
    });
}

function oldWebKitCommit()
{
    return new CommitLog(2, {
        id: 2,
        repository: MockModels.webkit,
        revision: '200574',
        time: +(new Date('2016-05-09T14:59:23.553767Z')),
    });
}

function gitWebKitCommit()
{
    return new CommitLog(3, {
        id: 3,
        repository: MockModels.webkit,
        revision: '6f8b0dbbda95a440503b88db1dd03dad3a7b07fb',
        time: +(new Date('2016-05-13T00:55:57.841344Z')),
    });
}

function oldGitWebKitCommit()
{
    return new CommitLog(4, {
        id: 4,
        repository: MockModels.webkit,
        revision: 'ffda14e6db0746d10d0f050907e4a7325851e502',
        time: +(new Date('2016-05-09T14:59:23.553767Z')),
    });
}

function osxCommit()
{
    return new CommitLog(5, {
        id: 5,
        repository: MockModels.osx,
        revision: '10.11.4 15E65',
        time: null,
        order: 1504065
    });
}

function osxCommitWithTestability()
{
    return new CommitLog(5, {
        id: 5,
        repository: MockModels.osx,
        revision: '10.11.4 15E65',
        time: null,
        order: 1504065,
        testability: "Panic on boot"
    });
}

function oldOSXCommit()
{
    return new CommitLog(6, {
        id: 6,
        repository: MockModels.osx,
        revision: '10.11.3 15D21',
        time: null,
        order: 1503021
    });
}

function commitWithoutOwnedCommits()
{
    return new CommitLog(6, {
        id: 6,
        repository: MockModels.ownerRepository,
        revision: '10.11.4 15E66',
        ownsCommits: false,
        time: null,
        order: 1504065
    });
}

function ownerCommit()
{
    return new CommitLog(5, {
        id: 5,
        repository: MockModels.ownerRepository,
        revision: '10.11.4 15E65',
        ownsCommits: true,
        time: null,
        order: 1504065
    });
}

function otherOwnerCommit()
{
    return new CommitLog(5, {
        id: 5,
        repository: MockModels.ownerRepository,
        revision: '10.11.4 15E66',
        ownsCommits: true,
        time: null,
        order: 1504066
    });
}

function ownedCommit()
{
    return new CommitLog(11, {
        id: 11,
        repository: MockModels.ownedRepository,
        revision: 'owned-commit-0',
        ownsCommits: true,
        time: null
    });
}

function anotherOwnedCommit()
{
    return new CommitLog(11, {
        id: 11,
        repository: MockModels.ownedRepository,
        revision: 'owned-commit-1',
        ownsCommits: true,
        time: null
    });
}

describe('CommitLog', function () {
    MockModels.inject();

    describe('label', function () {
        it('should prefix SVN revision with "r"', function () {
            assert.equal(webkitCommit().label(), 'r200805');
        });

        it('should truncate a Git hash at 8th character', function () {
            assert.equal(gitWebKitCommit().label(), '6f8b0dbbda95');
        });

        it('should not modify OS X version', function () {
            assert.equal(osxCommit().label(), '10.11.4 15E65');
        });
    });

    describe('title', function () {
        it('should prefix SVN revision with "r"', function () {
            assert.equal(webkitCommit().title(), 'WebKit at r200805');
        });

        it('should truncate a Git hash at 8th character', function () {
            assert.equal(gitWebKitCommit().title(), 'WebKit at 6f8b0dbbda95');
        });

        it('should not modify OS X version', function () {
            assert.equal(osxCommit().title(), 'OS X at 10.11.4 15E65');
        });
    });

    describe('order', () => {
        it('should return null if no commit order', () => {
            assert.equal(webkitCommit().order(), null);
        });
        it('should return commit order if order exists', () => {
            assert.equal(osxCommit().order(), 1504065);
        });
    });

    describe('diff', function () {
        it('should use label() as the label the previous commit is missing', function () {
            assert.deepEqual(webkitCommit().diff(), {
                label: 'r200805',
                url: 'http://trac.webkit.org/changeset/200805',
                repository: MockModels.webkit
            });

            assert.deepEqual(gitWebKitCommit().diff(), {
                label: '6f8b0dbbda95',
                url: 'http://trac.webkit.org/changeset/6f8b0dbbda95a440503b88db1dd03dad3a7b07fb',
                repository: MockModels.webkit,
            });

            assert.deepEqual(osxCommit().diff(), {
                label: '10.11.4 15E65',
                url: '',
                repository: MockModels.osx,
            });
        });

        it('should use increment the old SVN revision by 1', function () {
            assert.deepEqual(webkitCommit().diff(oldWebKitCommit()), {
                label: 'r200574-r200805',
                url: '',
                repository: MockModels.webkit
            });
        });

        it('should truncate a Git hash at 8th character', function () {
            assert.deepEqual(gitWebKitCommit().diff(oldGitWebKitCommit()), {
                label: 'ffda14e6db07..6f8b0dbbda95',
                url: '',
                repository: MockModels.webkit
            });
        });

        it('should surround "-" with spaces', function () {
            assert.deepEqual(osxCommit().diff(oldOSXCommit()), {
                label: '10.11.3 15D21 - 10.11.4 15E65',
                url: '',
                repository: MockModels.osx
            });
        });
    });

    describe('hasOrdering', () => {
        it('should return "true" when both commits have commit orders', () => {
            assert.ok(CommitLog.hasOrdering(osxCommit(), oldOSXCommit()));
        });

        it('should return "true" when both commits have commit time', () => {
            assert.ok(CommitLog.hasOrdering(webkitCommit(), oldWebKitCommit()));
        });

        it('should return "false" when neither commit time nor commit order exists', () => {
            assert.ok(!CommitLog.hasOrdering(ownedCommit(), anotherOwnedCommit()));
        });

        it('should return "false" when one commit only has commit time and another only has commit order', () => {
            assert.ok(!CommitLog.hasOrdering(webkitCommit(), osxCommit()));
        });
    });

    describe('hasCommitOrder', () => {
        it('should return "true" when a commit has commit order', () => {
            assert.ok(osxCommit().hasCommitOrder());
        });

        it('should return "false" when a commit only has commit time', () => {
            assert.ok(!webkitCommit().hasCommitOrder());
        });
    });

    describe('hasCommitTime', () => {
        it('should return "true" when a commit has commit order', () => {
            assert.ok(!osxCommit().hasCommitTime());
        });

        it('should return "false" when a commit only has commit time', () => {
            assert.ok(webkitCommit().hasCommitTime());
        });
    });

    describe('orderTowCommits', () => {
        it('should order by time when both commits have time', () => {
            const startCommit = oldWebKitCommit();
            const endCommit = webkitCommit();
            assert.deepEqual(CommitLog.orderTwoCommits(endCommit, startCommit), [startCommit, endCommit]);
            assert.deepEqual(CommitLog.orderTwoCommits(startCommit, endCommit), [startCommit, endCommit]);
        });

        it('should order by commit order when both commits only have commit order', () => {
            const startCommit = oldOSXCommit();
            const endCommit = osxCommit();
            assert.deepEqual(CommitLog.orderTwoCommits(endCommit, startCommit), [startCommit, endCommit]);
            assert.deepEqual(CommitLog.orderTwoCommits(startCommit, endCommit), [startCommit, endCommit]);
        });
    });

    describe('fetchOwnedCommits', () => {
        beforeEach(() => {
            MockRemoteAPI.inject(null, BrowserPrivilegedAPI);
        });

        it('should reject if repository of the commit does not own other repositories', () => {
            const commit = osxCommit();
            return commit.fetchOwnedCommits().then(() => {
               assert(false, 'Should not execute this line.');
            }, (error) => {
                assert.equal(error, undefined);
            });
        });

        it('should reject if commit does not own other owned-commits', () => {
            const commit = commitWithoutOwnedCommits();
            return commit.fetchOwnedCommits().then(() => {
                assert(false, 'Should not execute this line.');
            }, (error) => {
                assert.equal(error, undefined);
            });
        });

        it('should return owned-commit for a valid commit revision', () => {
            const commit = ownerCommit();
            const fetchingPromise = commit.fetchOwnedCommits();
            const requests = MockRemoteAPI.requests;
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../api/commits/111/owned-commits?owner-revision=10.11.4%2015E65');
            assert.equal(requests[0].method, 'GET');

            requests[0].resolve({commits: [{
                id: 233,
                repository: MockModels.ownedRepository.id(),
                revision: '6f8b0dbbda95a440503b88db1dd03dad3a7b07fb',
                time: +(new Date('2016-05-13T00:55:57.841344Z')),
            }]});
            return fetchingPromise.then((ownedCommits) => {
                assert.equal(ownedCommits.length, 1);
                assert.equal(ownedCommits[0].repository(), MockModels.ownedRepository);
                assert.equal(ownedCommits[0].revision(), '6f8b0dbbda95a440503b88db1dd03dad3a7b07fb');
                assert.equal(ownedCommits[0].id(), 233);
                assert.equal(ownedCommits[0].ownerCommit(), commit);
            });
        });

        it('should only fetch owned-commits exactly once', () => {
            const commit = ownerCommit();
            const fetchingPromise = commit.fetchOwnedCommits();
            const requests = MockRemoteAPI.requests;
            let existingOwnedCommits = null;
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../api/commits/111/owned-commits?owner-revision=10.11.4%2015E65');
            assert.equal(requests[0].method, 'GET');

            MockRemoteAPI.requests[0].resolve({commits: [{
                id: 233,
                repository: MockModels.ownedRepository.id(),
                revision: '6f8b0dbbda95a440503b88db1dd03dad3a7b07fb',
                time: +(new Date('2016-05-13T00:55:57.841344Z')),
            }]});

            return fetchingPromise.then((ownedCommits) => {
                existingOwnedCommits = ownedCommits;
                assert.equal(ownedCommits.length, 1);
                assert.equal(ownedCommits[0].repository(), MockModels.ownedRepository);
                assert.equal(ownedCommits[0].revision(), '6f8b0dbbda95a440503b88db1dd03dad3a7b07fb');
                assert.equal(ownedCommits[0].id(), 233);
                assert.equal(ownedCommits[0].ownerCommit(), commit);
                return commit.fetchOwnedCommits();
            }).then((ownedCommits) => {
                assert.equal(requests.length, 1);
                assert.equal(existingOwnedCommits, ownedCommits);
            });
        });
    });

    describe('ownedCommitDifferenceForOwnerCommits', () => {
        beforeEach(() => {
            MockRemoteAPI.reset();
        });

        it('should return difference between owned-commits of 2 owner commits', () => {
            const oneCommit = ownerCommit();
            const otherCommit = otherOwnerCommit();
            const fetchingPromise = oneCommit.fetchOwnedCommits();
            const requests = MockRemoteAPI.requests;
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../api/commits/111/owned-commits?owner-revision=10.11.4%2015E65');
            assert.equal(requests[0].method, 'GET');

            requests[0].resolve({commits: [{
                id: 233,
                repository: MockModels.ownedRepository.id(),
                revision: '6f8b0dbbda95a440503b88db1dd03dad3a7b07fb',
                time: +(new Date('2016-05-13T00:55:57.841344Z')),
            }, {
                id: 299,
                repository: MockModels.webkitGit.id(),
                revision: '04a6c72038f0b771a19248ca2549e1258617b5fc',
                time: +(new Date('2016-05-13T00:55:57.841344Z')),
            }]});

            return fetchingPromise.then((ownedCommits) => {
                assert.equal(ownedCommits.length, 2);
                assert.equal(ownedCommits[0].repository(), MockModels.ownedRepository);
                assert.equal(ownedCommits[0].revision(), '6f8b0dbbda95a440503b88db1dd03dad3a7b07fb');
                assert.equal(ownedCommits[0].id(), 233);
                assert.equal(ownedCommits[0].ownerCommit(), oneCommit);
                assert.equal(ownedCommits[1].repository(), MockModels.webkitGit);
                assert.equal(ownedCommits[1].revision(), '04a6c72038f0b771a19248ca2549e1258617b5fc');
                assert.equal(ownedCommits[1].id(), 299);
                assert.equal(ownedCommits[1].ownerCommit(), oneCommit);

                const otherFetchingPromise = otherCommit.fetchOwnedCommits();
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../api/commits/111/owned-commits?owner-revision=10.11.4%2015E66');
                assert.equal(requests[1].method, 'GET');

                requests[1].resolve({commits: [{
                    id: 234,
                    repository: MockModels.ownedRepository.id(),
                    revision: 'd5099e03b482abdd77f6c4dcb875afd05bda5ab8',
                    time: +(new Date('2016-05-13T00:55:57.841344Z')),
                }, {
                    id: 299,
                    repository: MockModels.webkitGit.id(),
                    revision: '04a6c72038f0b771a19248ca2549e1258617b5fc',
                    time: +(new Date('2016-05-13T00:55:57.841344Z')),
                }]});

                return otherFetchingPromise;
            }).then((ownedCommits) => {
                assert.equal(ownedCommits.length, 2);
                assert.equal(ownedCommits[0].repository(), MockModels.ownedRepository);
                assert.equal(ownedCommits[0].revision(), 'd5099e03b482abdd77f6c4dcb875afd05bda5ab8');
                assert.equal(ownedCommits[0].id(), 234);
                assert.equal(ownedCommits[0].ownerCommit(), otherCommit);
                assert.equal(ownedCommits[1].repository(), MockModels.webkitGit);
                assert.equal(ownedCommits[1].revision(), '04a6c72038f0b771a19248ca2549e1258617b5fc');
                assert.equal(ownedCommits[1].id(), 299);
                assert.equal(ownedCommits[1].ownerCommit(), otherCommit);
                const difference = CommitLog.ownedCommitDifferenceForOwnerCommits(oneCommit, otherCommit);
                assert.equal(difference.size, 1);
                assert.equal(difference.keys().next().value, MockModels.ownedRepository);
            });

        });
    });

    describe('testability', () => {
        it('should return "testability" message', () => {
            const commit = osxCommitWithTestability();
            assert.equal(commit.testability(), 'Panic on boot');
        });
    });

    const commitsAPIResponse = {
        "commits":[{
            "id": "831151",
            "revision": "236643",
            "repository": "11",
            "previousCommit": null,
            "time": 1538223077403,
            "order": null,
            "authorName": "Commit Queue",
            "authorEmail": "commit-queue@webkit.org",
            "message": "message",
            "ownsCommits": false
        }],
        "status":"OK"
    };

    const commitsAPIResponseForOwnedWebKit = {
        "commits":[{
            "id": "1831151",
            "revision": "236643",
            "repository": "191",
            "previousCommit": null,
            "time": 1538223077403,
            "order": null,
            "authorName": "Commit Queue",
            "authorEmail": "commit-queue@webkit.org",
            "message": "message",
            "ownsCommits": false
        }],
        "status":"OK"
    };

    describe('fetchForSingleRevision', () => {
        beforeEach(() => {
            MockRemoteAPI.reset();
        });

        it('should avoid fetching same revision from same repository twice', async () => {
            let fetchingPromise = CommitLog.fetchForSingleRevision(MockModels.webkit, '236643');

            const requests = MockRemoteAPI.requests;
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/api/commits/${MockModels.webkit.id()}/236643`);

            requests[0].resolve(commitsAPIResponse);
            const commits = await fetchingPromise;
            assert.equal(commits.length, 1);

            MockRemoteAPI.reset();
            assert.equal(requests.length, 0);
            fetchingPromise = CommitLog.fetchForSingleRevision(MockModels.webkit, '236643');
            assert.equal(requests.length, 0);
            const newCommits = await fetchingPromise;
            assert.equal(newCommits.length, 1);

            assert.equal(commits[0], newCommits[0]);
        });

        it('should avoid fetching same revision from same repository again if it has been fetched by "fetchBetweenRevisions" before', async () => {
            let fetchingPromise = CommitLog.fetchBetweenRevisions(MockModels.webkit, '236642', '236643');

            const requests = MockRemoteAPI.requests;
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/api/commits/${MockModels.webkit.id()}/?precedingRevision=236642&lastRevision=236643`);

            requests[0].resolve(commitsAPIResponse);
            const commits = await fetchingPromise;
            assert.equal(commits.length, 1);

            MockRemoteAPI.reset();
            assert.equal(requests.length, 0);
            fetchingPromise = CommitLog.fetchForSingleRevision(MockModels.webkit, '236643');
            assert.equal(requests.length, 0);
            const newCommits = await fetchingPromise;
            assert.equal(newCommits.length, 1);

            assert.equal(commits[0], newCommits[0]);
        });

        it('should not overwrite cache for commits with same revision but from different repositories', async () => {
            let fetchingPromise = CommitLog.fetchForSingleRevision(MockModels.webkit, '236643');

            const requests = MockRemoteAPI.requests;
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/api/commits/${MockModels.webkit.id()}/236643`);

            requests[0].resolve(commitsAPIResponse);
            const commits = await fetchingPromise;
            assert.equal(commits.length, 1);

            MockRemoteAPI.reset();
            assert.equal(requests.length, 0);

            fetchingPromise = CommitLog.fetchForSingleRevision(MockModels.ownedWebkit, '236643');
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/api/commits/${MockModels.ownedWebkit.id()}/236643`);

            requests[0].resolve(commitsAPIResponseForOwnedWebKit);
            const newCommits = await fetchingPromise;
            assert.equal(newCommits.length, 1);

            assert.notEqual(commits[0], newCommits[0]);
        });

        it('should allow to fetch commit with prefix', async () => {
            CommitLog.fetchForSingleRevision(MockModels.webkit, '236643', true);
            const requests = MockRemoteAPI.requests;
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/api/commits/${MockModels.webkit.id()}/236643?prefix-match=true`);
        });

        it('should not set prefix-match when "prefixMatch" is false', async () => {
            CommitLog.fetchForSingleRevision(MockModels.webkit, '236643', false);
            const requests = MockRemoteAPI.requests;
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/api/commits/${MockModels.webkit.id()}/236643`);
        })
    });
});
