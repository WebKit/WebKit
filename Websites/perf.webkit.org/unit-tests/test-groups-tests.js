'use strict';

var assert = require('assert');

require('../tools/js/v3-models.js');
let MockModels = require('./resources/mock-v3-models.js').MockModels;

function sampleTestGroup() {
    return {
        "testGroups": [{
            "id": "2128",
            "task": "1376",
            "name": "Confirm",
            "author": "rniwa",
            "createdAt": 1458688514000,
            "hidden": false,
            "buildRequests": ["16985", "16986", "16987", "16988", "16989", "16990", "16991", "16992"],
            "rootSets": ["4255", "4256", "4255", "4256", "4255", "4256", "4255", "4256"]
        }
        ],
        "buildRequests": [{
            "id": "16985",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "0",
            "rootSet": "4255",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16986",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "1",
            "rootSet": "4256",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        },
        {
            "id": "16987",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "2",
            "rootSet": "4255",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }, {
            "id": "16988",
            "triggerable": "3",
            "test": "844",
            "platform": "31",
            "testGroup": "2128",
            "order": "3",
            "rootSet": "4256",
            "status": "pending",
            "url": null,
            "build": null,
            "createdAt": 1458688514000
        }
        ],
        "rootSets": [{
            "id": "4255",
            "roots": ["87832", "93116"]
        }, {
            "id": "4256",
            "roots": ["87832", "96336"]
        }
        ],
        "roots": [{
            "id": "87832",
            "repository": "9",
            "revision": "10.11 15A284",
            "time": 0
        }, {
            "id": "93116",
            "repository": "11",
            "revision": "191622",
            "time": 1445945816878
        }, {
            "id": "87832",
            "repository": "9",
            "revision": "10.11 15A284",
            "time": 0
        }, {
            "id": "96336",
            "repository": "11",
            "revision": "192736",
            "time": 1448225325650
        }
        ],
        "status": "OK"
    };
}

describe('TestGroup', function () {
    MockModels.inject();

    describe('_createModelsFromFetchedTestGroups', function () {
        it('should create test groups', function () {
            var groups = TestGroup._createModelsFromFetchedTestGroups(sampleTestGroup());
            assert.equal(groups.length, 1);

            var group = groups[0];
            assert.ok(group instanceof TestGroup);
            assert.equal(group.id(), 2128);
            assert.ok(group.createdAt() instanceof Date);
            assert.equal(group.isHidden(), false);
            assert.equal(+group.createdAt(), 1458688514000);
            assert.equal(group.repetitionCount(), sampleTestGroup()['buildRequests'].length / 2);
            assert.ok(group.hasPending());
            assert.ok(!group.hasFinished());
            assert.ok(!group.hasStarted());
        });

        it('should not create a new instance of TestGroup object if there is a matching entry', function () {
            var firstObject = TestGroup._createModelsFromFetchedTestGroups(sampleTestGroup())[0];
            assert.ok(firstObject instanceof TestGroup);
            assert.equal(TestGroup._createModelsFromFetchedTestGroups(sampleTestGroup())[0], firstObject);
        });

        it('should create build requests for each group', function () {
            var groups = TestGroup._createModelsFromFetchedTestGroups(sampleTestGroup());
            assert.equal(groups.length, 1);
            assert.equal(groups[0].buildRequests().length, sampleTestGroup()['buildRequests'].length);

            var buildRequests = groups[0].buildRequests();
            assert.equal(buildRequests[0].id(), 16985);
            assert.equal(buildRequests[0].order(), 0);
            assert.ok(!buildRequests[0].hasFinished());
            assert.ok(!buildRequests[0].hasStarted());
            assert.ok(buildRequests[0].isPending());
            assert.equal(buildRequests[0].statusLabel(), 'Waiting to be scheduled');
            assert.equal(buildRequests[0].buildId(), null);
            assert.equal(buildRequests[0].result(), null);

            assert.equal(buildRequests[1].id(), 16986);
            assert.equal(buildRequests[1].order(), 1);
            assert.ok(!buildRequests[1].hasFinished());
            assert.ok(!buildRequests[1].hasStarted());
            assert.ok(buildRequests[1].isPending());
            assert.equal(buildRequests[1].statusLabel(), 'Waiting to be scheduled');
            assert.equal(buildRequests[1].buildId(), null);
            assert.equal(buildRequests[1].result(), null);
        });

        it('should create root sets for each group', function () {
            var buildRequests = TestGroup._createModelsFromFetchedTestGroups(sampleTestGroup())[0].buildRequests();

            var firstSet = buildRequests[0].rootSet();
            assert.ok(firstSet instanceof RootSet);
            assert.equal(firstSet, buildRequests[2].rootSet());

            var secondSet = buildRequests[1].rootSet();
            assert.ok(secondSet instanceof RootSet);
            assert.equal(secondSet, buildRequests[3].rootSet());

            assert.equal(firstSet.revisionForRepository(MockModels.webkit), '191622');
            var firstWebKitCommit = firstSet.commitForRepository(MockModels.webkit);
            assert.ok(firstWebKitCommit instanceof CommitLog);
            assert.ok(firstWebKitCommit, buildRequests[2].rootSet().commitForRepository(MockModels.webkit));
            assert.ok(firstWebKitCommit.repository(), MockModels.webkit);
            assert.ok(firstWebKitCommit.revision(), '191622');
            assert.ok(firstWebKitCommit.time() instanceof Date);
            assert.ok(+firstWebKitCommit.time(), 1445945816878);

            assert.equal(secondSet.revisionForRepository(MockModels.webkit), '192736');
            var secondWebKitCommit = secondSet.commitForRepository(MockModels.webkit);
            assert.ok(secondWebKitCommit instanceof CommitLog);
            assert.ok(secondWebKitCommit, buildRequests[3].rootSet().commitForRepository(MockModels.webkit));
            assert.ok(secondWebKitCommit.repository(), MockModels.webkit);
            assert.ok(secondWebKitCommit.revision(), '192736');
            assert.ok(secondWebKitCommit.time() instanceof Date);
            assert.ok(+secondWebKitCommit.time(), 1445945816878);

            assert.equal(firstSet.revisionForRepository(MockModels.osx), '10.11 15A284');
            var osxCommit = firstSet.commitForRepository(MockModels.osx);
            assert.ok(osxCommit instanceof CommitLog);
            assert.equal(osxCommit, buildRequests[1].rootSet().commitForRepository(MockModels.osx));
            assert.equal(osxCommit, buildRequests[2].rootSet().commitForRepository(MockModels.osx));
            assert.equal(osxCommit, buildRequests[3].rootSet().commitForRepository(MockModels.osx));
            assert.ok(osxCommit.repository(), MockModels.osx);
            assert.ok(osxCommit.revision(), '10.11 15A284');
        });
    });

    function testGroupWithStatusList(list) {
        var data = sampleTestGroup();
        data.buildRequests[0].status = list[0];
        data.buildRequests[1].status = list[1];
        data.buildRequests[2].status = list[2];
        data.buildRequests[3].status = list[3];
        return TestGroup._createModelsFromFetchedTestGroups(data)[0];
    }

    describe('hasFinished', function () {
        it('should return true if all build requests have completed', function () {
            assert.ok(testGroupWithStatusList(['completed', 'completed', 'completed', 'completed']).hasFinished());
        });

        it('should return true if all build requests have failed', function () {
            assert.ok(testGroupWithStatusList(['failed', 'failed', 'failed', 'failed']).hasFinished());
        });

        it('should return true if all build requests have been canceled', function () {
            assert.ok(testGroupWithStatusList(['canceled', 'canceled', 'canceled', 'canceled']).hasFinished());
        });

        it('should return true if all build requests have completed or failed', function () {
            assert.ok(testGroupWithStatusList(['failed', 'completed', 'failed', 'failed']).hasFinished());
        });

        it('should return true if all build requests have completed, failed, or canceled', function () {
            assert.ok(testGroupWithStatusList(['failed', 'completed', 'canceled', 'canceled']).hasFinished());
        });

        it('should return false if all build requests are pending', function () {
            assert.ok(!testGroupWithStatusList(['pending', 'pending', 'pending', 'pending']).hasFinished());
        });

        it('should return false if some build requests are pending', function () {
            assert.ok(!testGroupWithStatusList(['completed', 'completed', 'completed', 'pending']).hasFinished());
        });

        it('should return false if some build requests are scheduled', function () {
            assert.ok(!testGroupWithStatusList(['completed', 'completed', 'completed', 'scheduled']).hasFinished());
        });

        it('should return false if some build requests are running', function () {
            assert.ok(!testGroupWithStatusList(['completed', 'canceled', 'completed', 'running']).hasFinished());
        });
    });

    describe('hasStarted', function () {
        it('should return true if all build requests have completed', function () {
            assert.ok(testGroupWithStatusList(['completed', 'completed', 'completed', 'completed']).hasStarted());
        });

        it('should return true if all build requests have failed', function () {
            assert.ok(testGroupWithStatusList(['failed', 'failed', 'failed', 'failed']).hasStarted());
        });

        it('should return true if all build requests have been canceled', function () {
            assert.ok(testGroupWithStatusList(['canceled', 'canceled', 'canceled', 'canceled']).hasStarted());
        });

        it('should return true if all build requests have completed or failed', function () {
            assert.ok(testGroupWithStatusList(['failed', 'completed', 'failed', 'failed']).hasStarted());
        });

        it('should return true if all build requests have completed, failed, or cancled', function () {
            assert.ok(testGroupWithStatusList(['failed', 'completed', 'canceled', 'canceled']).hasStarted());
        });

        it('should return false if all build requests are pending', function () {
            assert.ok(!testGroupWithStatusList(['pending', 'pending', 'pending', 'pending']).hasStarted());
        });

        it('should return true if some build requests have completed', function () {
            assert.ok(testGroupWithStatusList(['completed', 'pending', 'pending', 'pending']).hasStarted());
        });

        it('should return true if some build requests are scheduled', function () {
            assert.ok(testGroupWithStatusList(['scheduled', 'pending', 'pending', 'pending']).hasStarted());
        });

        it('should return true if some build requests are running', function () {
            assert.ok(testGroupWithStatusList(['running', 'pending', 'pending', 'pending']).hasStarted());
        });
    });

    describe('hasPending', function () {
        it('should return false if all build requests have completed', function () {
            assert.ok(!testGroupWithStatusList(['completed', 'completed', 'completed', 'completed']).hasPending());
        });

        it('should return false if all build requests have failed', function () {
            assert.ok(!testGroupWithStatusList(['failed', 'failed', 'failed', 'failed']).hasPending());
        });

        it('should return false if all build requests have been canceled', function () {
            assert.ok(!testGroupWithStatusList(['canceled', 'canceled', 'canceled', 'canceled']).hasPending());
        });

        it('should return false if all build requests have completed or failed', function () {
            assert.ok(!testGroupWithStatusList(['failed', 'completed', 'failed', 'failed']).hasPending());
        });

        it('should return false if all build requests have completed, failed, or cancled', function () {
            assert.ok(!testGroupWithStatusList(['failed', 'completed', 'canceled', 'canceled']).hasPending());
        });

        it('should return true if all build requests are pending', function () {
            assert.ok(testGroupWithStatusList(['pending', 'pending', 'pending', 'pending']).hasPending());
        });

        it('should return true if some build requests are pending', function () {
            assert.ok(testGroupWithStatusList(['completed', 'failed', 'canceled', 'pending']).hasPending());
        });

        it('should return false if some build requests are scheduled and others have completed', function () {
            assert.ok(!testGroupWithStatusList(['completed', 'completed', 'completed', 'scheduled']).hasPending());
        });
    });

});