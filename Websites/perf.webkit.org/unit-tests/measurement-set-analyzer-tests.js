'use strict';

require('../tools/js/v3-models.js');
const assert = require('assert');
const assertThrows = require('../server-tests/resources/common-operations').assertThrows;
const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
const MockModels = require('./resources/mock-v3-models.js').MockModels;
const MeasurementSetAnalyzer = require('../tools/js/measurement-set-analyzer.js').MeasurementSetAnalyzer;
const NodePrivilegedAPI = require('../tools/js/privileged-api.js').PrivilegedAPI;

describe('MeasurementSetAnalyzer', () => {
    MockModels.inject();
    const requests = MockRemoteAPI.inject('http://build.webkit.org', NodePrivilegedAPI);

    describe('measurementSetListForAnalysis', () => {
        it('should generate empty list if no dashboard configurations', () => {
            const configurations =  MeasurementSetAnalyzer.measurementSetListForAnalysis({dashboards: {}});
            assert.equal(configurations.length, 0);
        });

        it('should generate a list of measurement set', () => {
            const configurations = MeasurementSetAnalyzer.measurementSetListForAnalysis({dashboards: {
                "macOS": [["some metric", "plt-mean"], [['Some Platform'], [65, 2884], [65, 1158]]]
            }});
            assert.equal(configurations.length, 2);
            const [measurementSet0, measurementSet1] = configurations;
            assert.equal(measurementSet0.metricId(), MockModels.someMetric.id());
            assert.equal(measurementSet0.platformId(), MockModels.somePlatform.id());
            assert.equal(measurementSet1.metricId(), MockModels.pltMean.id());
            assert.equal(measurementSet1.platformId(), MockModels.somePlatform.id());
        });
    });

    function mockLogger()
    {
        const info_logs = [];
        const error_logs =[];
        const warn_logs =[];
        return {
            info: (message) => info_logs.push(message),
            warn: (message) => warn_logs.push(message),
            error: (message) => error_logs.push(message),
            info_logs, warn_logs, error_logs
        };
    }

    describe('analyzeOnce', () => {
        const simpleSegmentableValues = [
            1546.5603, 1548.1536, 1563.5452, 1539.7823, 1546.4184, 1548.9299, 1532.5444, 1546.2800, 1547.1760, 1551.3507,
            1548.3277, 1544.7673, 1542.7157, 1538.1700, 1538.0948, 1543.0364, 1537.9737, 1542.2611, 1543.9685, 1546.4901,
            1544.4080, 1540.8671, 1537.3353, 1549.4331, 1541.4436, 1544.1299, 1550.1770, 1553.1872, 1549.3417, 1542.3788,
            1543.5094, 1541.7905, 1537.6625, 1547.3840, 1538.5185, 1549.6764, 1556.6138, 1552.0476, 1541.7629, 1544.7006,
            /* segments changes here */
            1587.1390, 1594.5451, 1586.2430, 1596.7310, 1548.1423
        ];

        const dataBeforeSmallProgression = [1587.1390, 1594.5451, 1586.2430, 1596.7310, 1548.1423];
        const dataBeforeHugeProgression = [1700.1390, 1704.5451, 1703.2430, 1706.7310, 1689.1423];

        function makeSampleRuns(values, startRunId, startTime, timeIncrement)
        {
            let runId = startRunId;
            let buildId = 3400;
            let buildNumber = 1;
            let commit_id = 1;
            let revision = 1;
            const makeRun = (value, commitTime) => [runId++, value, 1, value, value, false, [[commit_id++, MockModels.webkit.id(), revision++, 0, 0]], commitTime, commitTime + 10, buildId++, buildNumber++, MockModels.builder.id()];
            timeIncrement = Math.floor(timeIncrement);
            return values.map((value, index) => makeRun(value, startTime + index * timeIncrement));
        }

        it('should not analyze if no measurement set is available', async () => {
            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});
            await analysisPromise;
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====']);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should not analyze if no corresponding time series for a measurement set', async () => {
            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].reject('ConfigurationNotFound');

            try {
                await analysisPromise;
            } catch (error) {
                assert(false, 'Should not throw any exception here');
            }
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====']);
            assert.deepEqual(logger.warn_logs, [`Skipping analysis for "${MockModels.someMetric.fullName()}" on "${MockModels.somePlatform.name()}" as time series does not exit.`]);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should throw an error if "measurementSet.fetchBetween" is not failed due to "ConfugurationNotFound"', async () => {
            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].reject('SomeError');

            try {
                await analysisPromise;
            } catch (error) {
                assert.equal(error, 'SomeError');
            }
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====']);
            assert.deepEqual(logger.warn_logs, []);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should not analyze if there is only one data point in the measurement set', async () => {
            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: makeSampleRuns(simpleSegmentableValues.slice(0, 1), 6400, 4000, 1000 / 50)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});
            await analysisPromise;
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====']);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should not analyze if no regression is detected', async () => {
            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                'configurations': {current: makeSampleRuns(simpleSegmentableValues.slice(0, 39), 6400, 4000, 1000 / 50)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/api/analysis-tasks?platform=65&metric=2884');
            requests[1].resolve({
                analysisTasks: [],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await analysisPromise;
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====',
                'Nothing to analyze']);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should not show created analysis task logging if failed to create analysis task', async () => {
            PrivilegedAPI.configure('test', 'password');

            Triggerable.ensureSingleton(4, {name: 'some-triggerable',
                repositoryGroups: [MockModels.osRepositoryGroup, MockModels.svnRepositoryGroup, MockModels.gitRepositoryGroup, MockModels.svnRepositoryWithOwnedRepositoryGroup],
                configurations: [{test: MockModels.someMetric.test(), platform: MockModels.somePlatform}]});

            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            measurementSetAnalyzer._startTime = 4000;
            measurementSetAnalyzer._endTime = 5000;
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                'configurations': {current: makeSampleRuns(simpleSegmentableValues, 6400, 4000, 1000 / 50)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/api/analysis-tasks?platform=65&metric=2884');
            requests[1].resolve({
                analysisTasks: [],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 3);
            assert.equal(requests[2].url, '/privileged-api/create-analysis-task');
            assert.deepEqual(requests[2].data, {
                slaveName: 'test',
                slavePassword: 'password',
                name: 'Potential 2.38% regression on Some platform between WebKit: r35-r44',
                startRun: 6434,
                endRun: 6443,
                repetitionCount: 4,
                testGroupName: 'Confirm',
                needsNotification: true,
                revisionSets: [{'11': {revision: 35, ownerRevision: null, patch: null}},
                    {'11': {revision: 44, ownerRevision: null, patch: null}}]
            });
            requests[2].reject('TriggerableNotFoundForTask');

            assertThrows('TriggerableNotFoundForTask', async () => await analysisPromise);
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====']);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should analyze if a new regression is detected', async () => {
            PrivilegedAPI.configure('test', 'password');

            Triggerable.ensureSingleton(4, {name: 'some-triggerable',
                repositoryGroups: [MockModels.osRepositoryGroup, MockModels.svnRepositoryGroup, MockModels.gitRepositoryGroup, MockModels.svnRepositoryWithOwnedRepositoryGroup],
                configurations: [{test: MockModels.someMetric.test(), platform: MockModels.somePlatform}]});

            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            measurementSetAnalyzer._startTime = 4000;
            measurementSetAnalyzer._endTime = 5000;
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                'configurations': {current: makeSampleRuns(simpleSegmentableValues, 6400, 4000, 1000 / 50)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/api/analysis-tasks?platform=65&metric=2884');
            requests[1].resolve({
                analysisTasks: [],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 3);
            assert.equal(requests[2].url, '/privileged-api/create-analysis-task');
            assert.deepEqual(requests[2].data, {
                slaveName: 'test',
                slavePassword: 'password',
                name: 'Potential 2.38% regression on Some platform between WebKit: r35-r44',
                startRun: 6434,
                endRun: 6443,
                repetitionCount: 4,
                testGroupName: 'Confirm',
                needsNotification: true,
                revisionSets: [{'11': {revision: 35, ownerRevision: null, patch: null}},
                    {'11': {revision: 44, ownerRevision: null, patch: null}}]
            });
            requests[2].resolve({taskId: '5255', status: 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 4);
            assert.equal(requests[3].url, '/api/analysis-tasks?id=5255');

            requests[3].resolve({
                analysisTasks: [{
                    author: null,
                    bugs: [],
                    buildRequestCount: 8,
                    finishedBuildRequestCount: 0,
                    category: 'identified',
                    causes: [],
                    createdAt: 4500,
                    endRun: 6448,
                    endRunTime:  5000,
                    fixes: [],
                    id: 5255,
                    metric: MockModels.someMetric.id(),
                    name: 'Potential 2.38% regression on Some platform between WebKit: r40-r49',
                    needed: null,
                    platform: MockModels.somePlatform.id(),
                    result: 'regression',
                    segmentationStrategy: 1,
                    startRun: 6439,
                    startRunTime: 4000,
                    testRangeStrategy: 2
                }],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await analysisPromise;
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====',
                'Created analysis task with id "5255" to confirm: "Potential 2.38% regression on Some platform between WebKit: r35-r44".']);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should not create confirming A/B tests if a new regression is detected but no triggerable available', async () => {
            PrivilegedAPI.configure('test', 'password');
            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            measurementSetAnalyzer._startTime = 4000;
            measurementSetAnalyzer._endTime = 5000;
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                'configurations': {current: makeSampleRuns(simpleSegmentableValues, 6400, 4000, 1000 / 50)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/api/analysis-tasks?platform=65&metric=2884');
            requests[1].resolve({
                analysisTasks: [],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 3);
            assert.equal(requests[2].url, '/privileged-api/create-analysis-task');
            assert.deepEqual(requests[2].data, {
                slaveName: 'test',
                slavePassword: 'password',
                name: 'Potential 2.38% regression on Some platform between WebKit: r35-r44',
                startRun: 6434,
                endRun: 6443
            });
            requests[2].resolve({taskId: '5255', status: 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 4);
            assert.equal(requests[3].url, '/api/analysis-tasks?id=5255');

            requests[3].resolve({
                analysisTasks: [{
                    author: null,
                    bugs: [],
                    buildRequestCount: 0,
                    finishedBuildRequestCount: 0,
                    category: 'identified',
                    causes: [],
                    createdAt: 4500,
                    endRun: 6448,
                    endRunTime:  5000,
                    fixes: [],
                    id: 5255,
                    metric: MockModels.someMetric.id(),
                    name: 'Potential 2.38% regression on Some platform between WebKit: r40-r49',
                    needed: null,
                    platform: MockModels.somePlatform.id(),
                    result: 'regression',
                    segmentationStrategy: 1,
                    startRun: 6439,
                    startRunTime: 4000,
                    testRangeStrategy: 2
                }],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await analysisPromise;
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====',
                'Created analysis task with id "5255" to confirm: "Potential 2.38% regression on Some platform between WebKit: r35-r44".']);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should not analyze if there is an overlapped existing analysis task', async () => {
            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            measurementSetAnalyzer._startTime = 4000;
            measurementSetAnalyzer._endTime = 5000;
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                'configurations': {current: makeSampleRuns(simpleSegmentableValues, 6400, 4000, 1000 / 50)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/api/analysis-tasks?platform=65&metric=2884');
            requests[1].resolve({
                analysisTasks: [{
                    author: null,
                    bugs: [],
                    buildRequestCount: 14,
                    finishedBuildRequestCount: 6,
                    category: 'identified',
                    causes: [],
                    createdAt: 4500,
                    endRun: 6434,
                    endRunTime:  5000,
                    fixes: [],
                    id: 1082,
                    metric: MockModels.someMetric.id(),
                    name: 'Potential 2.38% regression on Some platform between WebKit: r35-r44',
                    needed: null,
                    platform: MockModels.somePlatform.id(),
                    result: 'regression',
                    segmentationStrategy: 1,
                    startRun: 6434,
                    startRunTime: 4000,
                    testRangeStrategy: 2
                }],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await analysisPromise;
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====',
                'Nothing to analyze']);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should favor regression if the progression is not big enough', async () => {
            PrivilegedAPI.configure('test', 'password');

            Triggerable.ensureSingleton(4, {name: 'some-triggerable',
                repositoryGroups: [MockModels.osRepositoryGroup, MockModels.svnRepositoryGroup, MockModels.gitRepositoryGroup, MockModels.svnRepositoryWithOwnedRepositoryGroup],
                configurations: [{test: MockModels.someMetric.test(), platform: MockModels.somePlatform}]});

            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            measurementSetAnalyzer._startTime = 4000;
            measurementSetAnalyzer._endTime = 5000;
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                'configurations': {current: makeSampleRuns(dataBeforeSmallProgression.concat(simpleSegmentableValues), 6400, 4000, 1000 / 50)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/api/analysis-tasks?platform=65&metric=2884');
            requests[1].resolve({
                analysisTasks: [],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 3);
            assert.equal(requests[2].url, '/privileged-api/create-analysis-task');
            assert.deepEqual(requests[2].data, {
                slaveName: 'test',
                slavePassword: 'password',
                name: 'Potential 2.38% regression on Some platform between WebKit: r40-r49',
                startRun: 6439,
                endRun: 6448,
                repetitionCount: 4,
                testGroupName: 'Confirm',
                needsNotification: true,
                revisionSets: [{'11': {revision: 40, ownerRevision: null, patch: null}},
                    {'11': {revision: 49, ownerRevision: null, patch: null}}]
            });
            requests[2].resolve({taskId: '5255', status: 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 4);
            assert.equal(requests[3].url, '/api/analysis-tasks?id=5255');

            requests[3].resolve({
                analysisTasks: [{
                    author: null,
                    bugs: [],
                    buildRequestCount: 8,
                    finishedBuildRequestCount: 0,
                    category: 'identified',
                    causes: [],
                    createdAt: 4500,
                    endRun: 6448,
                    endRunTime:  5000,
                    fixes: [],
                    id: 5255,
                    metric: MockModels.someMetric.id(),
                    name: 'Potential 2.38% regression on Some platform between WebKit: r40-r49',
                    needed: null,
                    platform: MockModels.somePlatform.id(),
                    result: 'regression',
                    segmentationStrategy: 1,
                    startRun: 6439,
                    startRunTime: 4000,
                    testRangeStrategy: 2
                }],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await analysisPromise;
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====',
                'Created analysis task with id "5255" to confirm: "Potential 2.38% regression on Some platform between WebKit: r40-r49".']);
            assert.deepEqual(logger.error_logs, []);
        });

        it('should choose analyze progression when it is big enough', async () => {
            PrivilegedAPI.configure('test', 'password');

            Triggerable.ensureSingleton(4, {name: 'some-triggerable',
                repositoryGroups: [MockModels.osRepositoryGroup, MockModels.svnRepositoryGroup, MockModels.gitRepositoryGroup, MockModels.svnRepositoryWithOwnedRepositoryGroup],
                configurations: [{test: MockModels.someMetric.test(), platform: MockModels.somePlatform}]});

            const measurementSet = MeasurementSet.findSet(MockModels.somePlatform.id(), MockModels.someMetric.id(), 5000);
            const logger = mockLogger();
            const measurementSetAnalyzer = new MeasurementSetAnalyzer([measurementSet], 4000, 5000, logger);
            measurementSetAnalyzer._startTime = 4000;
            measurementSetAnalyzer._endTime = 5000;
            const analysisPromise = measurementSetAnalyzer.analyzeOnce(measurementSet);

            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, `/data/measurement-set-${MockModels.somePlatform.id()}-${MockModels.someMetric.id()}.json`);
            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                'configurations': {current: makeSampleRuns(dataBeforeHugeProgression.concat(simpleSegmentableValues), 6400, 4000, 1000 / 50)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 2);
            assert.equal(requests[1].url, '/api/analysis-tasks?platform=65&metric=2884');
            requests[1].resolve({
                analysisTasks: [],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 3);
            assert.equal(requests[2].url, '/privileged-api/create-analysis-task');
            assert.deepEqual(requests[2].data, {
                slaveName: 'test',
                slavePassword: 'password',
                name: 'Potential 9.15% progression on Some platform between WebKit: r3-r8',
                startRun: 6402,
                endRun: 6407,
                repetitionCount: 4,
                testGroupName: 'Confirm',
                needsNotification: true,
                revisionSets: [{'11': {revision: 3, ownerRevision: null, patch: null}},
                    {'11': {revision: 8, ownerRevision: null, patch: null}}]
            });
            requests[2].resolve({taskId: '5255', status: 'OK'});

            await MockRemoteAPI.waitForRequest();
            assert.equal(requests.length, 4);
            assert.equal(requests[3].url, '/api/analysis-tasks?id=5255');

            requests[3].resolve({
                analysisTasks: [{
                    author: null,
                    bugs: [],
                    buildRequestCount: 8,
                    finishedBuildRequestCount: 0,
                    category: 'identified',
                    causes: [],
                    createdAt: 4500,
                    endRun: 6407,
                    endRunTime:  5000,
                    fixes: [],
                    id: 5255,
                    metric: MockModels.someMetric.id(),
                    name: 'Potential 9.15% progression on Some platform between WebKit: r3-r8',
                    needed: null,
                    platform: MockModels.somePlatform.id(),
                    result: 'progression',
                    segmentationStrategy: 1,
                    startRun: 6402,
                    startRunTime: 4000,
                    testRangeStrategy: 2
                }],
                bugs: [],
                commits: [],
                status: 'OK'
            });

            await analysisPromise;
            assert.deepEqual(logger.info_logs, ['==== "Some test : Some metric" on "Some platform" ====',
                'Created analysis task with id "5255" to confirm: "Potential 9.15% progression on Some platform between WebKit: r3-r8".']);
            assert.deepEqual(logger.error_logs, []);
        });
    });
});