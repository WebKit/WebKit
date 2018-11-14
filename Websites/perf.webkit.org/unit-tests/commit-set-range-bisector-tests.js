'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');
const BrowserPrivilegedAPI = require('../public/v3/privileged-api.js').PrivilegedAPI;
const MockModels = require('./resources/mock-v3-models.js').MockModels;
const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;

describe('CommitSetRangeBisector', () => {

    function makeCommit(id, repository, revision, time, order)
    {
        return CommitLog.ensureSingleton(id, {
            id,
            repository,
            revision,
            ownsCommits: false,
            time,
            order
        });
    }

    function sortedCommitSets()
    {
        return [
            CommitSet.ensureSingleton(1, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(11, MockModels.osx, 'osx-commit-1', 1), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(2, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 21), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(3, {
                revisionItems: [
                    { commit: makeCommit(2, MockModels.webkit, 'webkit-commit-2', 20), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 21), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(4, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(5, {
                revisionItems: [
                    { commit: makeCommit(6, MockModels.webkit, 'webkit-commit-6', 60), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false }
                ],
                customRoots: []}),
        ];
    }

    function sortedCommitSetsWithoutTimeOrOrder()
    {
        return [
            CommitSet.ensureSingleton(6, {
                revisionItems: [
                    { commit: makeCommit(101, MockModels.webkit, 'webkit-commit-101', 0), requiresBuild: false },
                    { commit: makeCommit(111, MockModels.osx, 'osx-commit-111', 0), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(7, {
                revisionItems: [
                    { commit: makeCommit(101, MockModels.webkit, 'webkit-commit-101', 0), requiresBuild: false },
                    { commit: makeCommit(112, MockModels.osx, 'osx-commit-112', 0), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(8, {
                revisionItems: [
                    { commit: makeCommit(102, MockModels.webkit, 'webkit-commit-102', 0), requiresBuild: false },
                    { commit: makeCommit(112, MockModels.osx, 'osx-commit-112', 0), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(9, {
                revisionItems: [
                    { commit: makeCommit(103, MockModels.webkit, 'webkit-commit-103', 0), requiresBuild: false },
                    { commit: makeCommit(113, MockModels.osx, 'osx-commit-113', 0), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(10, {
                revisionItems: [
                    { commit: makeCommit(106, MockModels.webkit, 'webkit-commit-106', 0), requiresBuild: false },
                    { commit: makeCommit(113, MockModels.osx, 'osx-commit-113', 0), requiresBuild: false }
                ],
                customRoots: []}),
        ];
    }

    function commitSetsWithSomeCommitsOnlyHaveOrder()
    {
        return [
            CommitSet.ensureSingleton(11, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(11, MockModels.osx, 'osx-commit-1', 1), requiresBuild: false },
                    { commit: makeCommit(201, MockModels.ownerRepository, 'owner-commit-1', 0, 1), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(12, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 21), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(13, {
                revisionItems: [
                    { commit: makeCommit(2, MockModels.webkit, 'webkit-commit-2', 20), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 21), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(14, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(15, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(203, MockModels.ownerRepository, 'owner-commit-3', 0, 3), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(16, {
                revisionItems: [
                    { commit: makeCommit(6, MockModels.webkit, 'webkit-commit-6', 60), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(203, MockModels.ownerRepository, 'owner-commit-3', 0, 3), requiresBuild: false }
                ],
                customRoots: []}),
        ];
    }

    function commitSetsWitCommitRollback()
    {
        return [
            CommitSet.ensureSingleton(11, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(11, MockModels.osx, 'osx-commit-1', 1), requiresBuild: false },
                    { commit: makeCommit(201, MockModels.ownerRepository, 'owner-commit-1', 0, 1), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(12, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 21), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(13, {
                revisionItems: [
                    { commit: makeCommit(2, MockModels.webkit, 'webkit-commit-2', 20), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 21), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(14, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(15, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(203, MockModels.ownerRepository, 'owner-commit-3', 0, 3), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(17, {
                revisionItems: [
                    { commit: makeCommit(6, MockModels.webkit, 'webkit-commit-6', 60), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(201, MockModels.ownerRepository, 'owner-commit-1', 0, 1), requiresBuild: false }
                ],
                customRoots: []}),
        ];
    }

    function commitSetsWithSomeHaveOwnedCommits()
    {
        return [
            CommitSet.ensureSingleton(11, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(11, MockModels.osx, 'osx-commit-1', 1), requiresBuild: false },
                    { commit: makeCommit(201, MockModels.ownerRepository, 'owner-commit-1', 0, 1), requiresBuild: false },
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(12, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 21), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(13, {
                revisionItems: [
                    { commit: makeCommit(2, MockModels.webkit, 'webkit-commit-2', 20), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 21), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(14, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(202, MockModels.ownerRepository, 'owner-commit-2', 0, 2), requiresBuild: false },
                    { commit: makeCommit(302, MockModels.ownedRepository, 'owned-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(15, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(203, MockModels.ownerRepository, 'owner-commit-3', 0, 3), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(16, {
                revisionItems: [
                    { commit: makeCommit(6, MockModels.webkit, 'webkit-commit-6', 60), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 31), requiresBuild: false },
                    { commit: makeCommit(203, MockModels.ownerRepository, 'owner-commit-3', 0, 3), requiresBuild: false }
                ],
                customRoots: []}),
        ];
    }

    function commitSetsWithSomeCommitsNotMonotonicallyIncrease()
    {
        return [
            CommitSet.ensureSingleton(17, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(18, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(11, MockModels.osx, 'osx-commit-1', 0, 1), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(19, {
                revisionItems: [
                    { commit: makeCommit(2, MockModels.webkit, 'webkit-commit-2', 20), requiresBuild: false },
                    { commit: makeCommit(11, MockModels.osx, 'osx-commit-1', 0, 1), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(20, {
                revisionItems: [
                    { commit: makeCommit(2, MockModels.webkit, 'webkit-commit-2', 20), requiresBuild: false },
                    { commit: makeCommit(12, MockModels.osx, 'osx-commit-2', 0, 2), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(21, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(13, MockModels.osx, 'osx-commit-3', 0, 3), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(22, {
                revisionItems: [
                    { commit: makeCommit(6, MockModels.webkit, 'webkit-commit-6', 60), requiresBuild: false },
                    { commit: makeCommit(11, MockModels.osx, 'osx-commit-1', 0, 1), requiresBuild: false }
                ],
                customRoots: []}),
        ];
    }

    function commitSetWithOnlySomeCommitsHaveOrdering()
    {
        return [
            CommitSet.ensureSingleton(23, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(111, MockModels.osx, 'osx-commit-111', 0), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(24, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(112, MockModels.osx, 'osx-commit-112', 0), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(25, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(113, MockModels.osx, 'osx-commit-113', 0), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(26, {
                revisionItems: [
                    { commit: makeCommit(3, MockModels.webkit, 'webkit-commit-3', 30), requiresBuild: false },
                    { commit: makeCommit(114, MockModels.osx, 'osx-commit-114', 0), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(27, {
                revisionItems: [
                    { commit: makeCommit(6, MockModels.webkit, 'webkit-commit-6', 60), requiresBuild: false },
                    { commit: makeCommit(113, MockModels.osx, 'osx-commit-113', 0), requiresBuild: false }
                ],
                customRoots: []}),
        ];
    }

    function commitSetsWithTime()
    {
        return [
            CommitSet.ensureSingleton(28, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(201, MockModels.osx, 'osx-commit-201', 8), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(29, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(203, MockModels.osx, 'osx-commit-203', 11), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(30, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(204, MockModels.osx, 'osx-commit-204', 12), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(31, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 10), requiresBuild: false },
                    { commit: makeCommit(205, MockModels.osx, 'osx-commit-205', 13), requiresBuild: false }
                ],
                customRoots: []}),
        ];
    }

    function createRoot()
    {
        return UploadedFile.ensureSingleton(456, {'createdAt': new Date('2017-05-01T21:03:27Z'), 'filename': 'root.dat', 'extension': '.dat', 'author': 'some user',
            size: 16452234, sha256: '03eed7a8494ab8794c44b7d4308e55448fc56f4d6c175809ba968f78f656d58d'});
    }

    function commitSetWithRoot()
    {
        return CommitSet.ensureSingleton(15, {
            revisionItems: [{ commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 1), requiresBuild: false }],
            customRoots: [createRoot()]
        });
    }

    function commitSet()
    {
        return CommitSet.ensureSingleton(16, {
            revisionItems: [{ commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 1), requiresBuild: false }],
            customRoots: []
        });
    }

    function commitSetsWithNoCommonRepository() {
        return [
            CommitSet.ensureSingleton(1, {
                revisionItems: [
                    { commit: makeCommit(1, MockModels.webkit, 'webkit-commit-1', 1), requiresBuild: false },
                    { commit: makeCommit(11, MockModels.osx, 'osx-commit-1', 1), requiresBuild: false }
                ],
                customRoots: []}),
            CommitSet.ensureSingleton(2, {
                revisionItems: [
                    { commit: makeCommit(2, MockModels.webkit, 'webkit-commit-1', 1), requiresBuild: false },
                    { commit: makeCommit(31, MockModels.ios, 'ios-commit-1', 1), requiresBuild: false },
                ],
                customRoots: []})
        ];
    }

    describe('commitSetClosestToMiddleOfAllCommits', () => {
        MockModels.inject();
        const requests = MockRemoteAPI.inject(null, BrowserPrivilegedAPI);

        it('should return "null" if no common repository found', async () => {
            const middleCommitSet = await CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits(commitSetsWithNoCommonRepository().splice(0, 2));
            assert.equal(middleCommitSet, null);
        });

        it('should return "null" to bisect commit set with root', async () => {
            const middleCommitSet = await CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([commitSet(), commitSetWithRoot()], [commitSet(), commitSetWithRoot()]);
            assert.equal(middleCommitSet, null);
        });

        it('should return "null" if no repository with time or order is found', async () => {
            const allCommitSets = sortedCommitSetsWithoutTimeOrOrder();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const middleCommitSet = await CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            assert.equal(middleCommitSet, null);
        });

        it('should throw exception when failed to fetch commit log', async () => {
            const allCommitSets = sortedCommitSets();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            const rejectReason = '404';
            requests[0].reject(rejectReason);
            let exceptionRaised = false;
            try {
                await promise;
            } catch (error) {
                exceptionRaised = true;
                assert.equal(error, rejectReason);
            }
            assert.ok(exceptionRaised);
        });

        it('should return "null" if no commit set is found other than the commit sets that define the range', async () => {
            const allCommitSets = sortedCommitSets();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const webkitId = MockModels.webkit.id();
            const osxId = MockModels.osx.id();
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], [startCommitSet, endCommitSet]);
            assert.equal(requests.length, 2);
            assert.equal(requests[0].url, '/api/commits/9/?precedingRevision=osx-commit-1&lastRevision=osx-commit-3');
            assert.equal(requests[1].url, '/api/commits/11/?precedingRevision=webkit-commit-1&lastRevision=webkit-commit-6');
            requests[0].resolve({
                'commits': [
                    {
                        repository: osxId,
                        id: 12,
                        revision: 'osx-commit-2',
                        ownsCommits: false,
                        time: 21
                    },
                    {
                        repository: osxId,
                        id: 13,
                        revision: 'osx-commit-3',
                        ownsCommits: false,
                        time: 31
                    }
                ]
            });
            requests[1].resolve({
                'commits': [
                    {
                        repository: webkitId,
                        id: 2,
                        revision: 'webkit-commit-2',
                        ownsCommits: false,
                        time: 20
                    },
                    {
                        repository: webkitId,
                        id: 3,
                        revision: 'webkit-commit-3',
                        ownsCommits: false,
                        time: 30
                    },
                    {
                        repository: webkitId,
                        id: 4,
                        revision: 'webkit-commit-4',
                        ownsCommits: false,
                        time: 40
                    },
                    {
                        repository: webkitId,
                        id: 5,
                        revision: 'webkit-commit-5',
                        ownsCommits: false,
                        time: 50
                    },
                    {
                        repository: webkitId,
                        id: 6,
                        revision: 'webkit-commit-6',
                        ownsCommits: false,
                        time: 60
                    },
                ]
            });

            assert.equal(await promise, null);
        });

        it('should return bisecting commit set point closest to the middle of revision range', async () => {
            const allCommitSets = sortedCommitSets();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            assert.equal(requests.length, 2);
            const osxFetchRequest = requests.find((fetch_request) => fetch_request.url === '/api/commits/9/?precedingRevision=osx-commit-1&lastRevision=osx-commit-3');
            const webkitFetchRequest = requests.find((fetch_request) => fetch_request.url === '/api/commits/11/?precedingRevision=webkit-commit-1&lastRevision=webkit-commit-6');
            const webkitId = MockModels.webkit.id();
            const osxId = MockModels.osx.id();
            webkitFetchRequest.resolve({
                'commits': [
                    {
                        repository: webkitId,
                        id: 2,
                        revision: 'webkit-commit-2',
                        ownsCommits: false,
                        time: 20
                    },
                    {
                        repository: webkitId,
                        id: 3,
                        revision: 'webkit-commit-3',
                        ownsCommits: false,
                        time: 30
                    },
                    {
                        repository: webkitId,
                        id: 4,
                        revision: 'webkit-commit-4',
                        ownsCommits: false,
                        time: 40
                    },
                    {
                        repository: webkitId,
                        id: 5,
                        revision: 'webkit-commit-5',
                        ownsCommits: false,
                        time: 50
                    },
                    {
                        repository: webkitId,
                        id: 6,
                        revision: 'webkit-commit-6',
                        ownsCommits: false,
                        time: 60
                    },
                ]
            });
            osxFetchRequest.resolve({
                'commits': [
                    {
                        repository: osxId,
                        id: 12,
                        revision: 'osx-commit-2',
                        ownsCommits: false,
                        time: 21
                    },
                    {
                        repository: osxId,
                        id: 13,
                        revision: 'osx-commit-3',
                        ownsCommits: false,
                        time: 31
                    }
                ]
            });

            assert.equal(await promise, allCommitSets[3]);
        });

        it('should return same bisection point even when two commit sets from original commit set have reverse order', async () => {
            const allCommitSets = sortedCommitSets();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([endCommitSet, startCommitSet], allCommitSets);
            const webkitId = MockModels.webkit.id();
            const osxId = MockModels.osx.id();
            requests[0].resolve({
                'commits': [
                    {
                        repository: webkitId,
                        id: 2,
                        revision: 'webkit-commit-2',
                        ownsCommits: false,
                        time: 20
                    },
                    {
                        repository: webkitId,
                        id: 3,
                        revision: 'webkit-commit-3',
                        ownsCommits: false,
                        time: 30
                    },
                    {
                        repository: webkitId,
                        id: 4,
                        revision: 'webkit-commit-4',
                        ownsCommits: false,
                        time: 40
                    },
                    {
                        repository: webkitId,
                        id: 5,
                        revision: 'webkit-commit-5',
                        ownsCommits: false,
                        time: 50
                    },
                    {
                        repository: webkitId,
                        id: 6,
                        revision: 'webkit-commit-6',
                        ownsCommits: false,
                        time: 60
                    },
                ]
            });
            requests[1].resolve({
                'commits': [
                    {
                        repository: osxId,
                        id: 12,
                        revision: 'osx-commit-2',
                        ownsCommits: false,
                        time: 21
                    },
                    {
                        repository: osxId,
                        id: 13,
                        revision: 'osx-commit-3',
                        ownsCommits: false,
                        time: 31
                    }
                ]
            });

            assert.equal(await promise, allCommitSets[3]);
        });

        it('should use commits with order as fallback when multiple commit sets found for the commit that is closest to the middle of commits with time', async () => {
            const allCommitSets = commitSetsWithSomeCommitsOnlyHaveOrder();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            const webkitId = MockModels.webkit.id();
            const osxId = MockModels.osx.id();
            const ownerRepositoryId = MockModels.ownerRepository.id();

            assert.equal(requests.length, 3);
            assert.equal(requests[0].url, '/api/commits/9/?precedingRevision=osx-commit-1&lastRevision=osx-commit-3');
            assert.equal(requests[1].url, '/api/commits/111/?precedingRevision=owner-commit-1&lastRevision=owner-commit-3');
            assert.equal(requests[2].url, '/api/commits/11/?precedingRevision=webkit-commit-1&lastRevision=webkit-commit-6');

            requests[0].resolve({
                'commits': [
                    {
                        repository: osxId,
                        id: 12,
                        revision: 'osx-commit-2',
                        ownsCommits: false,
                        time: 21
                    },
                    {
                        repository: osxId,
                        id: 13,
                        revision: 'osx-commit-3',
                        ownsCommits: false,
                        time: 31
                    }
                ]
            });

            requests[1].resolve({
                'commits': [
                    {
                        repository: ownerRepositoryId,
                        id: 202,
                        revision: 'owner-commit-2',
                        ownsCommits: false,
                        time: 0,
                        order: 2
                    },
                    {
                        repository: ownerRepositoryId,
                        id: 203,
                        revision: 'owner-commit-3',
                        ownsCommits: false,
                        time: 0,
                        order: 3
                    }
                ]
            });

            requests[2].resolve({
                'commits': [
                    {
                        repository: webkitId,
                        id: 2,
                        revision: 'webkit-commit-2',
                        ownsCommits: false,
                        time: 20
                    },
                    {
                        repository: webkitId,
                        id: 3,
                        revision: 'webkit-commit-3',
                        ownsCommits: false,
                        time: 30
                    },
                    {
                        repository: webkitId,
                        id: 4,
                        revision: 'webkit-commit-4',
                        ownsCommits: false,
                        time: 40
                    },
                    {
                        repository: webkitId,
                        id: 5,
                        revision: 'webkit-commit-5',
                        ownsCommits: false,
                        time: 50
                    },
                    {
                        repository: webkitId,
                        id: 6,
                        revision: 'webkit-commit-6',
                        ownsCommits: false,
                        time: 60
                    },
                ]
            });
            assert.equal(await promise, allCommitSets[3]);
        });

        it('should still check the repository revision even the repository has no change in the range', async () => {
            const allCommitSets = commitSetsWitCommitRollback();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            const webkitId = MockModels.webkit.id();
            const osxId = MockModels.osx.id();

            assert.equal(requests.length, 2);
            assert.equal(requests[0].url, '/api/commits/9/?precedingRevision=osx-commit-1&lastRevision=osx-commit-3');
            assert.equal(requests[1].url, '/api/commits/11/?precedingRevision=webkit-commit-1&lastRevision=webkit-commit-6');

            requests[0].resolve({
                'commits': [
                    {
                        repository: osxId,
                        id: 12,
                        revision: 'osx-commit-2',
                        ownsCommits: false,
                        time: 21
                    },
                    {
                        repository: osxId,
                        id: 13,
                        revision: 'osx-commit-3',
                        ownsCommits: false,
                        time: 31
                    }
                ]
            });

            requests[1].resolve({
                'commits': [
                    {
                        repository: webkitId,
                        id: 2,
                        revision: 'webkit-commit-2',
                        ownsCommits: false,
                        time: 20
                    },
                    {
                        repository: webkitId,
                        id: 3,
                        revision: 'webkit-commit-3',
                        ownsCommits: false,
                        time: 30
                    },
                    {
                        repository: webkitId,
                        id: 4,
                        revision: 'webkit-commit-4',
                        ownsCommits: false,
                        time: 40
                    },
                    {
                        repository: webkitId,
                        id: 5,
                        revision: 'webkit-commit-5',
                        ownsCommits: false,
                        time: 50
                    },
                    {
                        repository: webkitId,
                        id: 6,
                        revision: 'webkit-commit-6',
                        ownsCommits: false,
                        time: 60
                    },
                ]
            });
            assert.equal(await promise, null);
        });

        it('should filter out commit set with owned commit', async () => {
            const allCommitSets = commitSetsWithSomeHaveOwnedCommits();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            const webkitId = MockModels.webkit.id();
            const osxId = MockModels.osx.id();
            const ownerRepositoryId = MockModels.ownerRepository.id();

            assert.equal(requests.length, 3);
            assert.equal(requests[0].url, '/api/commits/9/?precedingRevision=osx-commit-1&lastRevision=osx-commit-3');
            assert.equal(requests[1].url, '/api/commits/111/?precedingRevision=owner-commit-1&lastRevision=owner-commit-3');
            assert.equal(requests[2].url, '/api/commits/11/?precedingRevision=webkit-commit-1&lastRevision=webkit-commit-6');

            requests[0].resolve({
                'commits': [
                    {
                        repository: osxId,
                        id: 12,
                        revision: 'osx-commit-2',
                        ownsCommits: false,
                        time: 21
                    },
                    {
                        repository: osxId,
                        id: 13,
                        revision: 'osx-commit-3',
                        ownsCommits: false,
                        time: 31
                    }
                ]
            });

            requests[1].resolve({
                'commits': [
                    {
                        repository: ownerRepositoryId,
                        id: 202,
                        revision: 'owner-commit-2',
                        ownsCommits: true,
                        time: 0,
                        order: 2
                    },
                    {
                        repository: ownerRepositoryId,
                        id: 203,
                        revision: 'owner-commit-3',
                        ownsCommits: true,
                        time: 0,
                        order: 3
                    }
                ]
            });

            requests[2].resolve({
                'commits': [
                    {
                        repository: webkitId,
                        id: 2,
                        revision: 'webkit-commit-2',
                        ownsCommits: false,
                        time: 20
                    },
                    {
                        repository: webkitId,
                        id: 3,
                        revision: 'webkit-commit-3',
                        ownsCommits: false,
                        time: 30
                    },
                    {
                        repository: webkitId,
                        id: 4,
                        revision: 'webkit-commit-4',
                        ownsCommits: false,
                        time: 40
                    },
                    {
                        repository: webkitId,
                        id: 5,
                        revision: 'webkit-commit-5',
                        ownsCommits: false,
                        time: 50
                    },
                    {
                        repository: webkitId,
                        id: 6,
                        revision: 'webkit-commit-6',
                        ownsCommits: false,
                        time: 60
                    },
                ]
            });
            assert.equal(await promise, allCommitSets[4]);
        });

        it('should filter out commits those are not in any commit sets for commit without ordering', async () => {
            const allCommitSets = commitSetWithOnlySomeCommitsHaveOrdering();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            const webkitId = MockModels.webkit.id();
            const osxId = MockModels.osx.id();

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/commits/11/?precedingRevision=webkit-commit-1&lastRevision=webkit-commit-6');

            requests[0].resolve({
                'commits': [
                    {
                        repository: webkitId,
                        id: 2,
                        revision: 'webkit-commit-2',
                        ownsCommits: false,
                        time: 20
                    },
                    {
                        repository: webkitId,
                        id: 3,
                        revision: 'webkit-commit-3',
                        ownsCommits: false,
                        time: 30
                    },
                    {
                        repository: webkitId,
                        id: 4,
                        revision: 'webkit-commit-4',
                        ownsCommits: false,
                        time: 40
                    },
                    {
                        repository: webkitId,
                        id: 5,
                        revision: 'webkit-commit-5',
                        ownsCommits: false,
                        time: 50
                    },
                    {
                        repository: webkitId,
                        id: 6,
                        revision: 'webkit-commit-6',
                        ownsCommits: false,
                        time: 60
                    },
                ]
            });
            assert.equal(await promise, allCommitSets[2]);
        });

        it('should still work even some commits do not monotonically increasing', async () => {
            const allCommitSets = commitSetsWithSomeCommitsNotMonotonicallyIncrease();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            const webkitId = MockModels.webkit.id();
            const osxId = MockModels.osx.id();

            assert.equal(requests.length, 2);
            assert.equal(requests[0].url, '/api/commits/9/?precedingRevision=osx-commit-1&lastRevision=osx-commit-2');
            assert.equal(requests[1].url, '/api/commits/11/?precedingRevision=webkit-commit-1&lastRevision=webkit-commit-6');

            requests[0].resolve({
                'commits': [
                    {
                        repository: osxId,
                        id: 12,
                        revision: 'osx-commit-2',
                        ownsCommits: false,
                        time: 0,
                        order: 2
                    }
                ]
            });
            requests[1].resolve({
                'commits': [
                    {
                        repository: webkitId,
                        id: 2,
                        revision: 'webkit-commit-2',
                        ownsCommits: false,
                        time: 20
                    },
                    {
                        repository: webkitId,
                        id: 3,
                        revision: 'webkit-commit-3',
                        ownsCommits: false,
                        time: 30
                    },
                    {
                        repository: webkitId,
                        id: 4,
                        revision: 'webkit-commit-4',
                        ownsCommits: false,
                        time: 40
                    },
                    {
                        repository: webkitId,
                        id: 5,
                        revision: 'webkit-commit-5',
                        ownsCommits: false,
                        time: 50
                    },
                    {
                        repository: webkitId,
                        id: 6,
                        revision: 'webkit-commit-6',
                        ownsCommits: false,
                        time: 60
                    },
                ]
            });

            assert.equal(await promise, allCommitSets[3]);
        });

        it('should not double count a commit if start and end commits are the same', async () => {
            const allCommitSets = commitSetsWithTime();
            const startCommitSet = allCommitSets[0];
            const endCommitSet = allCommitSets[allCommitSets.length - 1];
            const promise = CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits([startCommitSet, endCommitSet], allCommitSets);
            assert.equal(requests.length, 1);
            const osxFetchRequest = requests[0];
            assert.equal(osxFetchRequest.url, '/api/commits/9/?precedingRevision=osx-commit-201&lastRevision=osx-commit-205');
            const osxId = MockModels.osx.id();
            osxFetchRequest.resolve({
                'commits': [
                    {
                        repository: osxId,
                        id: 202,
                        revision: 'osx-commit-202',
                        ownsCommits: false,
                        time: 9
                    },
                    {
                        repository: osxId,
                        id: 203,
                        revision: 'osx-commit-203',
                        ownsCommits: false,
                        time: 11
                    },
                    {
                        repository: osxId,
                        id: 204,
                        revision: 'osx-commit-204',
                        ownsCommits: false,
                        time: 12
                    },
                    {
                        repository: osxId,
                        id: 205,
                        revision: 'osx-commit-205',
                        ownsCommits: false,
                        time: 13
                    }
                ]
            });

            assert.equal(await promise, allCommitSets[1]);
        });
    });

    describe('_orderCommitSetsByTimeAndOrderThenDeduplicate', () => {
        it('should sort by alphabetically for commits without ordering', () => {
            const oneCommitSet = CommitSet.ensureSingleton(100, {
                revisionItems: [
                    { commit: makeCommit(1001, MockModels.webkit, 'webkit-commit-1001'), requiresBuild: false}
                ],
                customRoots: []
            });

            const anotherCommitSet = CommitSet.ensureSingleton(101, {
                revisionItems: [
                    { commit: makeCommit(1002, MockModels.webkit, 'webkit-commit-1002'), requiresBuild: false}
                ],
                customRoots: []
            });

            const [commitSet0, commitSet1] = CommitSetRangeBisector._orderCommitSetsByTimeAndOrderThenDeduplicate([anotherCommitSet, oneCommitSet], [], [], [MockModels.webkit]);
            assert.equal(oneCommitSet, commitSet0);
            assert.equal(anotherCommitSet, commitSet1);
        })
    })
});