'use strict';

var assert = require('assert');

require('../tools/js/v3-models.js');
let MockModels = require('./resources/mock-v3-models.js').MockModels;

const sampleCluster = {
    'clusterStart': 946684800000,
    'clusterSize': 5184000000,
    'configurations': {
        'current': [
            [28954983, 217.94607142857, 20, 4358.9214285714, 950303.02365434, false, [[111, 9, '10.11 15D21', 1504021, 0], [222, 11, '192483', null, 1447707055576], [333, 999, 'some unknown revision', null, 0]], 1447707055576, 184629, 1447762266153, '178', 176],
            [28952257, 220.11455357143, 20, 4402.2910714286, 969099.67509885, false, [[111, 9, '10.11 15D21', 1504021, 0], [444, 11, '192486', null, 1447713500460]], 1447713500460, 184614, 1447760255683, '177', 176]
        ],
        'baseline': [
            [10548956, 312.59, 1, 0, 0, false, [], 1420070400000, 67724, 1420070400000, "0", 0]
        ]
    },
    'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions',
        'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
    'startTime': 1449532800000,
    'endTime': 1454716800000,
    'lastModified': 1452067190008,
    'clusterCount': 2,
    'elapsedTime': 210.68406105042,
    'status': 'OK'
};
const sampleData = sampleCluster.configurations.current[0];
const sampleCustomBaselineData = sampleCluster.configurations.baseline[0];

describe('MeasurementAdaptor', function () {
    MockModels.inject();

    describe('applyTo', function () {
        it('should adapt id', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            assert.equal(adaptor.applyTo(sampleData).id, 28954983);
        });

        it('should adapt mean, squareMean, and iteration count', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            assert.equal(adaptor.applyTo(sampleData).value, 217.94607142857);
            assert.equal(adaptor.applyTo(sampleData).sum, 4358.9214285714);
            assert.equal(adaptor.applyTo(sampleData).squareSum, 950303.02365434);
            assert.equal(adaptor.applyTo(sampleData).iterationCount, 20);
        });

        it('should adapt commitTime as the canonical time', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            assert.equal(adaptor.applyTo(sampleData).time, 1447707055576);
        });

        it('should adapt build information as a Build object', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            assert.ok(adaptor.applyTo(sampleData).build() instanceof Build);
            assert.equal(adaptor.applyTo(sampleData).build().id(), 184629);
            assert.equal(adaptor.applyTo(sampleData).build().buildNumber(), '178');
            assert.equal(adaptor.applyTo(sampleData).build().builder(), MockModels.builder);
            assert.equal(adaptor.applyTo(sampleData).build().label(), 'Build 178 on WebKit Perf Builder');
            assert.equal(adaptor.applyTo(sampleData).build().url(), 'http://build.webkit.org/builders/WebKit Perf Builder/178');
        });

        it('should adapt build information as null when the builder is not specified', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            assert.equal(adaptor.applyTo(sampleCustomBaselineData).build(), null);
        });

        it('should adapt revision information as a CommitSet object', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            var commitSet = adaptor.applyTo(sampleData).commitSet();
            assert.ok(commitSet instanceof CommitSet);
            assert.equal(commitSet.latestCommitTime(), 1447707055576);
        });

        it('should adapt OS X version as a CommitLog object', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            var commitSet = adaptor.applyTo(sampleData).commitSet();
            assert.ok(commitSet instanceof CommitSet);
            assert.equal(commitSet.latestCommitTime(), 1447707055576);

            assert.ok(commitSet.repositories().indexOf(MockModels.osx) >= 0);
            assert.equal(commitSet.revisionForRepository(MockModels.osx), '10.11 15D21');

            var commit = commitSet.commitForRepository(MockModels.osx);
            assert.ok(commit instanceof CommitLog);
            assert.equal(commit.repository(), MockModels.osx);
            assert.ok(commit.time() instanceof Date);
            assert.equal(commit.id(), 111);
            assert.equal(commit.revision(), '10.11 15D21');
            assert.equal(commit.label(), '10.11 15D21');
            assert.equal(commit.title(), 'OS X at 10.11 15D21');
            assert.equal(commit.url(), '');
        });

        it('should adapt WebKit revision as a CommitLog object', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            var commitSet = adaptor.applyTo(sampleData).commitSet();

            assert.ok(commitSet.repositories().indexOf(MockModels.webkit) >= 0);
            assert.equal(commitSet.revisionForRepository(MockModels.webkit), '192483');

            var commit = commitSet.commitForRepository(MockModels.webkit);
            assert.ok(commit instanceof CommitLog);
            assert.equal(commit.repository(), MockModels.webkit);
            assert.ok(commit.time() instanceof Date);
            assert.equal(commit.id(), 222);
            assert.equal(+commit.time(), 1447707055576);
            assert.equal(commit.revision(), '192483');
            assert.equal(commit.label(), 'r192483');
            assert.equal(commit.title(), 'WebKit at r192483');
            assert.equal(commit.url(), 'http://trac.webkit.org/changeset/192483');
        });

        it('should not create separate CommitLog object for the same revision', function () {
            var adaptor = new MeasurementAdaptor(sampleCluster.formatMap);
            assert.equal(adaptor.applyTo(sampleData).commitSet().commitForRepository(MockModels.webkit),
                adaptor.applyTo(sampleData).commitSet().commitForRepository(MockModels.webkit));

            assert.equal(adaptor.applyTo(sampleData).commitSet().commitForRepository(MockModels.osx),
                adaptor.applyTo(sampleData).commitSet().commitForRepository(MockModels.osx));
        });

    });
});