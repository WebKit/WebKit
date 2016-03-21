'use strict';

var assert = require('assert');

require('./resources/v3-models.js');
require('./resources/mock-remote-api.js');

function sampleAnalysisTask()
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
                'parent': null,
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
                            1454481204649
                        ],
                        [
                            105975,
                            11,
                            '196051',
                            1454481246108
                        ],
                        [
                            105502,
                            9,
                            '10.11 15D21',
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

describe('AnalysisTask', function () {
    describe('fetchAll', function () {
        it('should request all analysis tasks', function () {
            var callCount = 0;
            AnalysisTask.fetchAll().then(function () { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../api/analysis-tasks');
        });

        it('should not request all analysis tasks multiple times', function () {
            var callCount = 0;
            AnalysisTask.fetchAll().then(function () { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../api/analysis-tasks');

            AnalysisTask.fetchAll().then(function () { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
        });

        it('should resolve the promise when the request is fullfilled', function (done) {
            var callCount = 0;
            var promise = AnalysisTask.fetchAll().then(function () { callCount++; });
            assert.equal(callCount, 0);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../api/analysis-tasks');

            requests[0].resolve(sampleAnalysisTask());

            var anotherCallCount = 0;
            return promise.then(function () {
                assert.equal(callCount, 1);
                AnalysisTask.fetchAll().then(function () { anotherCallCount++; });
            }).then(function () {
                assert.equal(callCount, 1);
                assert.equal(anotherCallCount, 1);
                assert.equal(requests.length, 1);
                done();
            }).catch(function (error) { done(error); });
        });

        it('should create AnalysisTask objects', function (done) {
            var promise = AnalysisTask.fetchAll();
            requests[0].resolve(sampleAnalysisTask());

            return promise.then(function () {
                assert.equal(AnalysisTask.all().length, 1);
                var task = AnalysisTask.all()[0];
                assert.equal(task.id(), 1082);
                assert.equal(task.metric(), someMetric);
                assert.equal(task.platform(), somePlatform);
                assert.ok(task.hasResults());
                assert.ok(task.hasPendingRequests());
                assert.equal(task.requestLabel(), '6 of 14');
                assert.equal(task.category(), 'identified');
                assert.equal(task.changeType(), 'regression');
                assert.equal(task.startMeasurementId(), 37117949);
                assert.equal(task.startTime(), 1454444458791);
                assert.equal(task.endMeasurementId(), 37253448);
                assert.equal(task.endTime(), 1454515020303);
                done();
            }).catch(function (error) { done(error); });
        });

        it('should create CommitLog objects for `causes`', function (done) {
            var promise = AnalysisTask.fetchAll();
            requests[0].resolve(sampleAnalysisTask());

            return promise.then(function () {
                assert.equal(AnalysisTask.all().length, 1);
                var task = AnalysisTask.all()[0];

                assert.equal(task.causes().length, 1);
                var commit = task.causes()[0];

                assert.equal(commit.revision(), '196051');
                assert.equal(commit.repository(), webkit);
                assert.equal(+commit.time(), 1454481246108);
                done();
            }).catch(function (error) { done(error); });
        });

        it('should find CommitLog objects for `causes` when MeasurementAdaptor created matching objects', function (done) {
            var adoptor = new MeasurementAdaptor(measurementCluster().formatMap);
            var adoptedMeasurement = adoptor.applyTo(measurementCluster().configurations.current[0]);
            assert.equal(adoptedMeasurement.id, 37188161);
            assert.equal(adoptedMeasurement.rootSet().commitForRepository(webkit).revision(), '196051');

            var promise = AnalysisTask.fetchAll();
            requests[0].resolve(sampleAnalysisTask());

            return promise.then(function () {
                assert.equal(AnalysisTask.all().length, 1);
                var task = AnalysisTask.all()[0];

                assert.equal(task.causes().length, 1);
                var commit = task.causes()[0];
                assert.equal(commit.revision(), '196051');
                assert.equal(commit.repository(), webkit);
                assert.equal(+commit.time(), 1454481246108);
                done();
            }).catch(function (error) { done(error); });
        });
    });
});
