'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');
const MockModels = require('./resources/mock-v3-models.js').MockModels;
const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
const BrowserPrivilegedAPI = require('../public/v3/privileged-api.js').PrivilegedAPI;
const NodePrivilegedAPI = require('../tools/js/privileged-api').PrivilegedAPI;

function sampleAnalysisTasks()
{
    return {
        'analysisTasks': [
            {
                'author': null,
                'bugs': [],
                'buildRequestCount': '14',
                'finishedBuildRequestCount': '6',
                'category': 'identified',
                'causes': [
                    '105975'
                ],
                'createdAt': 1454594330000,
                'endRun': '37253448',
                'endRunTime': 1454515020303,
                'fixes': [],
                'id': '1082',
                'metric': '2884',
                'name': 'Potential 1.2% regression between 2016-02-02 20:20 and 02-03 15:57',
                'needed': null,
                'platform': '65',
                'result': 'regression',
                'segmentationStrategy': '1',
                'startRun': '37117949',
                'startRunTime': 1454444458791,
                'testRangeStragegy': '2'
            }
        ],
        'bugs': [],
        'commits': [
            {
                'authorEmail': 'commit-queue@webkit.org',
                'authorName': 'Commit Queue',
                'id': '105975',
                'message': 'Commit message',
                'order': null,
                'previousCommit': null,
                'repository': '11',
                'revision': '196051',
                'time': 1454481246108
            }
        ],
        'status': 'OK'
    };
}

function measurementCluster()
{
    return {
        'clusterSize': 5184000000,
        'clusterStart': 946684800000,
        'configurations': {
            'current': [
                [
                    37188161,
                    124.15015662116,
                    25,
                    3103.7539155291,
                    385398.06003414,
                    false,
                    [
                        [
                            105978,
                            10,
                            '791451',
                            null,
                            1454481204649
                        ],
                        [
                            105975,
                            11,
                            '196051',
                            null,
                            1454481246108
                        ],
                        [
                            105502,
                            9,
                            '10.11 15D21',
                            1504021,
                            0
                        ]
                    ],
                    1454481246108,
                    227020,
                    1454492139496,
                    '3151',
                    179
                ],
            ],
        },
        'endTime': 1454716800000,
        'formatMap': [
            'id',
            'mean',
            'iterationCount',
            'sum',
            'squareSum',
            'markedOutlier',
            'revisions',
            'commitTime',
            'build',
            'buildTime',
            'buildNumber',
            'builder'
        ],
        'lastModified': 1455236216153,
        'startTime': 1449532800000,
        'status': 'OK'
    };
}

describe('AnalysisTask', () => {
    MockModels.inject();

    function makeMockPoints(id, commitSet) {
        return {
            id,
            commitSet: () => commitSet
        }
    }

    describe('fetchById', () => {
        const requests = MockRemoteAPI.inject(null, BrowserPrivilegedAPI);
        it('should fetch the specified analysis task', () => {
            let callCount = 0;
            AnalysisTask.fetchById(1).then(() => { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/analysis-tasks?id=1');
        });

        it('should not fetch the specified analysis task if all analysis task had been fetched', async () => {
            const fetchAll = AnalysisTask.fetchAll();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/analysis-tasks');
            requests[0].resolve(sampleAnalysisTasks());

            await fetchAll;
            AnalysisTask.fetchById(sampleAnalysisTasks().analysisTasks[0].id);
            assert.equal(requests.length, 1);
        });

        it('should fetch the specified analysis task if all analysis task had been fetched but noCache is set to true', async () => {
            const fetchAll = AnalysisTask.fetchAll();
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/analysis-tasks');
            requests[0].resolve(sampleAnalysisTasks());

            await fetchAll;
            const taskId = sampleAnalysisTasks().analysisTasks[0].id;
            AnalysisTask.fetchById(taskId, true);
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, `/api/analysis-tasks?id=${taskId}`);
        });

    })

    describe('fetchAll', () => {
        const requests = MockRemoteAPI.inject(null, BrowserPrivilegedAPI);
        it('should request all analysis tasks', () => {
            let callCount = 0;
            AnalysisTask.fetchAll().then(() => { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/analysis-tasks');
        });

        it('should not request all analysis tasks multiple times', () => {
            let callCount = 0;
            AnalysisTask.fetchAll().then(() => { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/analysis-tasks');

            AnalysisTask.fetchAll().then(() => { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
        });

        it('should resolve the promise when the request is fullfilled', () => {
            let callCount = 0;
            const promise = AnalysisTask.fetchAll().then(() => { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/api/analysis-tasks');

            requests[0].resolve(sampleAnalysisTasks());

            let anotherCallCount = 0;
            return promise.then(() => {
                assert.equal(callCount, 1);
                AnalysisTask.fetchAll().then(() => { anotherCallCount++; });
            }).then(() => {
                assert.equal(callCount, 1);
                assert.equal(anotherCallCount, 1);
                assert.equal(requests.length, 1);
            });
        });

        it('should create AnalysisTask objects', () => {
            const promise = AnalysisTask.fetchAll();
            requests[0].resolve(sampleAnalysisTasks());

            return promise.then(() => {
                assert.equal(AnalysisTask.all().length, 1);
                var task = AnalysisTask.all()[0];
                assert.equal(task.id(), 1082);
                assert.equal(task.metric(), MockModels.someMetric);
                assert.equal(task.platform(), MockModels.somePlatform);
                assert.ok(task.hasResults());
                assert.ok(task.hasPendingRequests());
                assert.equal(task.requestLabel(), '6 of 14');
                assert.equal(task.category(), 'investigated');
                assert.equal(task.changeType(), 'regression');
                assert.equal(task.startMeasurementId(), 37117949);
                assert.equal(task.startTime(), 1454444458791);
                assert.equal(task.endMeasurementId(), 37253448);
                assert.equal(task.endTime(), 1454515020303);
            });
        });

        it('should create CommitLog objects for `causes`', () => {
            const promise = AnalysisTask.fetchAll();
            requests[0].resolve(sampleAnalysisTasks());

            return promise.then(() => {
                assert.equal(AnalysisTask.all().length, 1);
                var task = AnalysisTask.all()[0];

                assert.equal(task.causes().length, 1);
                var commit = task.causes()[0];

                assert.equal(commit.revision(), '196051');
                assert.equal(commit.repository(), MockModels.webkit);
                assert.equal(+commit.time(), 1454481246108);
            });
        });

        it('should find CommitLog objects for `causes` when MeasurementAdaptor created matching objects', () => {
            const adaptor = new MeasurementAdaptor(measurementCluster().formatMap);
            const adaptedMeasurement = adaptor.applyTo(measurementCluster().configurations.current[0]);
            assert.equal(adaptedMeasurement.id, 37188161);
            assert.equal(adaptedMeasurement.commitSet().commitForRepository(MockModels.webkit).revision(), '196051');

            const promise = AnalysisTask.fetchAll();
            requests[0].resolve(sampleAnalysisTasks());

            return promise.then(() => {
                assert.equal(AnalysisTask.all().length, 1);
                var task = AnalysisTask.all()[0];

                assert.equal(task.causes().length, 1);
                var commit = task.causes()[0];
                assert.equal(commit.revision(), '196051');
                assert.equal(commit.repository(), MockModels.webkit);
                assert.equal(+commit.time(), 1454481246108);
            });
        });
    });


    function mockStartAndEndPoints() {
        const startPoint = makeMockPoints(1, new MeasurementCommitSet(1, [
            [1, MockModels.ios.id(), 'ios-revision-1', null, 0],
            [3, MockModels.webkit.id(), 'webkit-revision-1', null, 0]
        ]));
        const endPoint = makeMockPoints(2, new MeasurementCommitSet(2, [
            [2, MockModels.ios.id(), 'ios-revision-2', null, 0],
            [4, MockModels.webkit.id(), 'webkit-revision-2', null, 0]
        ]));
        return [startPoint, endPoint];
    }

    describe('create with browser privilege api', () => {
        const requests = MockRemoteAPI.inject(null, BrowserPrivilegedAPI);

        it('should create analysis task with confirming repetition count zero as default with browser privilege api', async () => {
            const [startPoint, endPoint] = mockStartAndEndPoints();
            AnalysisTask.create('confirm', startPoint, endPoint);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests[1].url, '/privileged-api/create-analysis-task');
            assert.equal(requests.length, 2);
            assert.deepEqual(requests[1].data, {name: 'confirm', startRun: 1, endRun: 2, token: 'abc'});
        });

        it('should create analysis task with confirming repetition count specified', async () => {
            const [startPoint, endPoint] = mockStartAndEndPoints();
            AnalysisTask.create('confirm', startPoint, endPoint, 'Confirm', 4, true);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests[1].url, '/privileged-api/create-analysis-task');
            assert.equal(requests.length, 2);
            assert.deepEqual(requests[1].data, {name: 'confirm', repetitionCount: 4, needsNotification: true,
                startRun: 1, endRun: 2, testGroupName: 'Confirm', token: 'abc', revisionSets: [
                    {'11': {revision: 'webkit-revision-1', ownerRevision: null, patch: null},
                        '22': {revision: 'ios-revision-1', ownerRevision: null, patch: null}},
                    {'11': {revision: 'webkit-revision-2', ownerRevision: null, patch: null},
                        '22': { revision: 'ios-revision-2', ownerRevision: null, patch: null}}]}
            );
        });

        it('should create analysis task and test groups with "needsNotification" set to false if specified in creation', async () => {
            const [startPoint, endPoint] = mockStartAndEndPoints();
            AnalysisTask.create('confirm', startPoint, endPoint, 'Confirm', 4, false);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests[1].url, '/privileged-api/create-analysis-task');
            assert.equal(requests.length, 2);
            assert.deepEqual(requests[1].data, {name: 'confirm', repetitionCount: 4, needsNotification: false,
                startRun: 1, endRun: 2, testGroupName: 'Confirm', token: 'abc', revisionSets: [
                    {'11': {revision: 'webkit-revision-1', ownerRevision: null, patch: null},
                        '22': {revision: 'ios-revision-1', ownerRevision: null, patch: null}},
                    {'11': {revision: 'webkit-revision-2', ownerRevision: null, patch: null},
                        '22': { revision: 'ios-revision-2', ownerRevision: null, patch: null}}]}
            );
        });

        it('should sync the new analysis task status once it is created', async () => {
            const [startPoint, endPoint] = mockStartAndEndPoints();
            const creatingPromise = AnalysisTask.create('confirm', startPoint, endPoint, 'Confirm', 4, true);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests[1].url, '/privileged-api/create-analysis-task');
            assert.equal(requests.length, 2);
            assert.deepEqual(requests[1].data, {name: 'confirm', repetitionCount: 4, needsNotification: true,
                startRun: 1, endRun: 2, testGroupName: 'Confirm', token: 'abc', revisionSets: [
                    {'11': {revision: 'webkit-revision-1', ownerRevision: null, patch: null},
                        '22': {revision: 'ios-revision-1', ownerRevision: null, patch: null}},
                    {'11': {revision: 'webkit-revision-2', ownerRevision: null, patch: null},
                        '22': { revision: 'ios-revision-2', ownerRevision: null, patch: null}}]}
            );

            requests[1].resolve({taskId: '5255', status: 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 3);
            assert.equal(requests[2].url, '/api/analysis-tasks?id=5255');
            requests[2].resolve({
                analysisTasks: [{
                    author: null,
                    bugs: [],
                    buildRequestCount: 8,
                    finishedBuildRequestCount: 0,
                    category: 'identified',
                    causes: [],
                    createdAt: 4500,
                    endRun: 2,
                    endRunTime:  5000,
                    fixes: [],
                    id: 5255,
                    metric: MockModels.someMetric.id(),
                    name: 'confirm',
                    needed: null,
                    platform: MockModels.somePlatform.id(),
                    result: 'progression',
                    segmentationStrategy: 1,
                    startRun: 1,
                    startRunTime: 4000,
                    testRangeStrategy: 2
                }],
                bugs: [],
                commits: [],
                status: 'OK'
            });
            const analysisTask = await creatingPromise;
            assert.equal(analysisTask.id(), 5255);
            assert.deepEqual(analysisTask.bugs(), []);
            assert.equal(analysisTask.author(), '');
            assert.equal(analysisTask.platform(), MockModels.somePlatform);
            assert.equal(analysisTask.metric(), MockModels.someMetric);
        });

        it('should return an rejected promise when analysis task creation failed', async () => {
            const [startPoint, endPoint] = mockStartAndEndPoints();
            const creatingPromise = AnalysisTask.create('confirm', startPoint, endPoint, 'Confirm', 4, true);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '/privileged-api/generate-csrf-token');
            requests[0].resolve({
                token: 'abc',
                expiration: Date.now() + 3600 * 1000,
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests[1].url, '/privileged-api/create-analysis-task');
            assert.equal(requests.length, 2);
            assert.deepEqual(requests[1].data, {name: 'confirm', repetitionCount: 4, needsNotification: true,
                startRun: 1, endRun: 2, testGroupName: 'Confirm', token: 'abc', revisionSets: [
                    {'11': {revision: 'webkit-revision-1', ownerRevision: null, patch: null},
                        '22': {revision: 'ios-revision-1', ownerRevision: null, patch: null}},
                    {'11': {revision: 'webkit-revision-2', ownerRevision: null, patch: null},
                        '22': { revision: 'ios-revision-2', ownerRevision: null, patch: null}}]}
            );

            requests[1].reject('401');
            return creatingPromise.then(() => {
                assert.ok(false, 'should not be reached');
            }, (error) => {
                assert.ok(true);
                assert.equal(error, '401');
            });
        });
    });

    describe('create with node privilege api', () => {
        const requests = MockRemoteAPI.inject(null, NodePrivilegedAPI);
        beforeEach(() => {
            PrivilegedAPI.configure('worker', 'password');
        });

        it('should create analysis task with confirming repetition count zero as default with browser privilege api', () => {
            const [startPoint, endPoint] = mockStartAndEndPoints();
            AnalysisTask.create('confirm', startPoint, endPoint);
            assert.equal(requests[0].url, '/privileged-api/create-analysis-task');
            assert.equal(requests.length, 1);
            assert.deepEqual(requests[0].data, {name: 'confirm', startRun: 1, endRun: 2, slaveName: 'worker', slavePassword: 'password'});
        });

        it('should create analysis task with confirming repetition count specified', () => {
            const [startPoint, endPoint] = mockStartAndEndPoints();
            AnalysisTask.create('confirm', startPoint, endPoint, 'Confirm', 4, true);
            assert.equal(requests[0].url, '/privileged-api/create-analysis-task');
            assert.equal(requests.length, 1);
            assert.deepEqual(requests[0].data, {name: 'confirm', repetitionCount: 4, needsNotification: true,
                startRun: 1, endRun: 2, slaveName: 'worker', slavePassword: 'password',
                testGroupName: 'Confirm', revisionSets: [
                    {'11': {revision: 'webkit-revision-1', ownerRevision: null, patch: null},
                        '22': {revision: 'ios-revision-1', ownerRevision: null, patch: null}},
                    {'11': {revision: 'webkit-revision-2', ownerRevision: null, patch: null},
                        '22': { revision: 'ios-revision-2', ownerRevision: null, patch: null}}]}
            );
        });
    });
});
