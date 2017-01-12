'use strict';

const assert = require('assert');
if (!assert.almostEqual)
    assert.almostEqual = require('./resources/almost-equal.js');

const MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
require('../tools/js/v3-models.js');
const MockModels = require('./resources/mock-v3-models.js').MockModels;

describe('MeasurementSet', function () {
    MockModels.inject();
    let requests = MockRemoteAPI.inject();

    beforeEach(function () {
        MeasurementSet._set = null;
    });

    function waitForMeasurementSet()
    {
        return Promise.resolve().then(function () {
            return Promise.resolve();
        }).then(function () {
            return Promise.resolve();
        });
    }

    describe('findSet', function () {
        it('should create a new MeasurementSet for a new pair of platform and matric', function () {
            assert.notEqual(MeasurementSet.findSet(1, 1, 3000), MeasurementSet.findSet(1, 2, 3000));
            assert.notEqual(MeasurementSet.findSet(1, 1, 3000), MeasurementSet.findSet(2, 1, 3000));
        });

        it('should not create a new MeasurementSet when the same pair of platform and matric are requested', function () {
            assert.equal(MeasurementSet.findSet(1, 1), MeasurementSet.findSet(1, 1));
        });
    });

    describe('findClusters', function () {

        it('should return clusters that exist', function (done) {
            var set = MeasurementSet.findSet(1, 1, 1467852503940);
            var callCount = 0;
            var promise = set.fetchBetween(1465084800000, 1470268800000, function () {
                callCount++;
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 946684800000,
                'clusterSize': 5184000000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 1465084800000,
                'endTime': 1470268800000,
                'lastModified': 1467852503940,
                'clusterCount': 5,
                'status': 'OK'});

            promise.then(function () {
                assert.deepEqual(set.findClusters(0, Date.now()), [1449532800000, 1454716800000, 1459900800000, 1465084800000, 1470268800000]);
                done();
            }).catch(function (error) {
                done(error);
            });
        });

    });

    describe('fetchBetween', function () {
        it('should always request the cached primary cluster first', function () {
            var set = MeasurementSet.findSet(1, 1, 3000);
            set.fetchBetween(1000, 2000, function () { assert.notReached(); });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');
        });

        it('should not request the cached primary cluster twice', function () {
            var set = MeasurementSet.findSet(1, 1, 3000);
            set.fetchBetween(1000, 2000, function () { assert.notReached(); });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');
            set.fetchBetween(2000, 3000, function () { assert.notReached(); });
            assert.equal(requests.length, 1);
        });

        it('should invoke the callback when the up-to-date cached primary cluster is fetched and it matches the requested range', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3000);
            var callCount = 0;
            var promise = set.fetchBetween(2000, 3000, function () {
                callCount++;
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 3000,
                'clusterCount': 2,
                'status': 'OK'});

            promise.then(function () {
                assert.equal(callCount, 1);
                assert.equal(requests.length, 1);
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should invoke the callback and fetch a secondary cluster when the cached primary cluster is up-to-date and within in the requested range', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3000);
            var callCount = 0;
            var promise = set.fetchBetween(1000, 3000, function () {
                callCount++;
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 3000,
                'clusterCount': 2,
                'status': 'OK'});

            waitForMeasurementSet().then(function () {
                assert.equal(callCount, 1);
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-2000.json');
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should request additional secondary clusters as requested', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            set.fetchBetween(2000, 3000, function () {
                assert.notReached();
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

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

            var callCount = 0;
            waitForMeasurementSet().then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-3000.json');

                set.fetchBetween(0, 7000, function () { callCount++; });

                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCount, 1);
                assert.equal(requests.length, 4);
                assert.equal(requests[2].url, '../data/measurement-set-1-1-2000.json');
                assert.equal(requests[3].url, '../data/measurement-set-1-1-4000.json');

                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should request secondary clusters which forms a superset of the requested range', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            set.fetchBetween(2707, 4207, function () {
                assert.notReached();
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 3,
                'status': 'OK'});

            waitForMeasurementSet().then(function () {
                assert.equal(requests.length, 3);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-3000.json');
                assert.equal(requests[2].url, '../data/measurement-set-1-1-4000.json');
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should not request secondary clusters that are not requested', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            set.fetchBetween(3200, 3700, function () {
                assert.notReached();
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

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

            var callCount = 0;
            waitForMeasurementSet().then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-4000.json');
                set.fetchBetween(1207, 1293, function () { callCount++; });
                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCount, 0);
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '../data/measurement-set-1-1-2000.json');
                set.fetchBetween(1964, 3401, function () { callCount++; });
                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCount, 0);
                assert.equal(requests.length, 4);
                assert.equal(requests[3].url, '../data/measurement-set-1-1-3000.json');
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should not request a cluster before the very first cluster', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            set.fetchBetween(0, 3000, function () {
                assert.notReached();
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 2000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 5000,
                'clusterCount': 1,
                'status': 'OK'});

            var callCount = 0;
            waitForMeasurementSet().then(function () {
                assert.equal(requests.length, 1);
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should invoke the callback when the fetching of the primray cluster fails', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3000);
            var callCount = 0;
            set.fetchBetween(1000, 3000, function () {
                callCount++;
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].reject(500);

            waitForMeasurementSet().then(function () {
                assert.equal(callCount, 1);
                assert.equal(requests.length, 1);
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should request the uncached primary cluster when the cached cluster is outdated', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3005);
            var callCount = 0;
            set.fetchBetween(1000, 2000, function () {
                callCount++;
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 3000,
                'clusterCount': 2,
                'status': 'OK'});

            Promise.resolve().then(function () {
                assert.equal(callCount, 0);
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../api/measurement-set?platform=1&metric=1');
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should request the uncached primary cluster when the cached cluster is 404', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3005);
            var callCount = 0;
            set.fetchBetween(1000, 2000, function () {
                callCount++;
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].reject(404);

            waitForMeasurementSet().then(function () {
                assert.equal(callCount, 0);
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../api/measurement-set?platform=1&metric=1');
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should request the uncached primary cluster when noCache is true', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3000);
            var callCount = 0;
            set.fetchBetween(1000, 3000, function () {
                callCount++;
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 3000,
                'clusterCount': 2,
                'status': 'OK'});

            var noCacheFetchCount = 0;
            waitForMeasurementSet().then(function () {
                assert.equal(callCount, 1);
                assert.equal(noCacheFetchCount, 0);
                assert.equal(set._sortedClusters.length, 1);
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-2000.json');

                requests[1].resolve({
                    'clusterStart': 1000,
                    'clusterSize': 1000,
                    'formatMap': [],
                    'configurations': {current: []},
                    'startTime': 1000,
                    'endTime': 2000,
                    'lastModified': 3000,
                    'clusterCount': 2,
                    'status': 'OK'});

                set.fetchBetween(1000, 3000, function () {
                    noCacheFetchCount++;
                }, true /* noCache */);

                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCount, 2);
                assert.equal(noCacheFetchCount, 0);
                assert.equal(set._sortedClusters.length, 2);
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '../api/measurement-set?platform=1&metric=1');

                requests[2].resolve({
                    'clusterStart': 1000,
                    'clusterSize': 1000,
                    'formatMap': [],
                    'configurations': {current: []},
                    'startTime': 2000,
                    'endTime': 3000,
                    'lastModified': 3000,
                    'clusterCount': 2,
                    'status': 'OK'});

                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCount, 2);
                assert.equal(noCacheFetchCount, 1);
                assert.equal(set._sortedClusters.length, 2);
                assert.equal(requests.length, 4);
                assert.equal(requests[3].url, '../data/measurement-set-1-1-2000.json');

                requests[3].resolve({
                    'clusterStart': 1000,
                    'clusterSize': 1000,
                    'formatMap': [],
                    'configurations': {current: []},
                    'startTime': 1000,
                    'endTime': 2000,
                    'lastModified': 3000,
                    'clusterCount': 2,
                    'status': 'OK'});

                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCount, 3);
                assert.equal(noCacheFetchCount, 2);
                assert.equal(set._sortedClusters.length, 2);
                assert.equal(requests.length, 4);

                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should not request the primary cluster twice when multiple clients request it but should invoke all callbacks', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3000);
            var callCount = 0;
            set.fetchBetween(2000, 3000, function () {
                callCount++;
            });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            var alternativeCallCount = 0;
            set.fetchBetween(2000, 3000, function () {
                alternativeCallCount++;
            });

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 3000,
                'clusterCount': 2,
                'status': 'OK'});

            waitForMeasurementSet().then(function () {
                assert.equal(callCount, 1);
                assert.equal(alternativeCallCount, 1);
                assert.equal(requests.length, 1);
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should invoke callback for each secondary clusters that are fetched or rejected', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            var callCountFor4000 = 0;
            set.fetchBetween(3200, 3700, function () { callCountFor4000++; });
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

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

            var callCountFor4000To5000 = 0;
            var callCountFor2000 = 0;
            var callCountFor2000To4000 = 0;
            waitForMeasurementSet().then(function () {
                assert.equal(callCountFor4000, 0);
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-4000.json');

                set.fetchBetween(3708, 4800, function () { callCountFor4000To5000++; });
                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCountFor4000To5000, 1);
                assert.equal(requests.length, 2);

                set.fetchBetween(1207, 1293, function () { callCountFor2000++; });
                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCountFor2000, 0);
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '../data/measurement-set-1-1-2000.json');

                requests[2].resolve({
                    'formatMap': [],
                    'configurations': {current: []},
                    'startTime': 1000,
                    'endTime': 2000,
                    'lastModified': 5000,
                    'status': 'OK'});
                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(requests.length, 3);
                assert.equal(callCountFor4000, 0);
                assert.equal(callCountFor4000To5000, 1);
                assert.equal(callCountFor2000, 1);

                set.fetchBetween(1964, 3401, function () { callCountFor2000To4000++; });
                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCountFor2000To4000, 1);
                assert.equal(requests.length, 4);
                assert.equal(requests[3].url, '../data/measurement-set-1-1-3000.json');

                requests[3].resolve({
                    'formatMap': [],
                    'configurations': {current: []},
                    'startTime': 2000,
                    'endTime': 3000,
                    'lastModified': 5000,
                    'status': 'OK'});
                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCountFor4000, 0);
                assert.equal(callCountFor4000To5000, 1);
                assert.equal(callCountFor2000, 1);
                assert.equal(callCountFor2000To4000, 2);
                assert.equal(requests.length, 4);

                requests[1].resolve({
                    'formatMap': [],
                    'configurations': {current: []},
                    'startTime': 3000,
                    'endTime': 4000,
                    'lastModified': 5000,
                    'status': 'OK'});
                return waitForMeasurementSet();
            }).then(function () {
                assert.equal(callCountFor4000, 1);
                assert.equal(callCountFor4000To5000, 2);
                assert.equal(callCountFor2000, 1);
                assert.equal(callCountFor2000To4000, 3);
                assert.equal(requests.length, 4);

                done();
            }).catch(function (error) {
                done(error);
            })
        });

    });

    describe('hasFetchedRange', function () {

        it('should return false when no clusters had been fetched', function () {
            var set = MeasurementSet.findSet(1, 1, 3000);
            assert(!set.hasFetchedRange(2000, 3000));
        });

        it('should return true when a single cluster contains the entire range', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3000);
            var promise = set.fetchBetween(2000, 3000);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 3000,
                'clusterCount': 2,
                'status': 'OK'});

            promise.then(function () {
                assert(set.hasFetchedRange(2001, 2999));
                assert(set.hasFetchedRange(2000, 3000));
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should return false when the range starts before the fetched cluster', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3000);
            var promise = set.fetchBetween(2000, 3000);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 3000,
                'clusterCount': 2,
                'status': 'OK'});

            promise.then(function () {
                assert(!set.hasFetchedRange(1500, 3000));
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should return false when the range ends after the fetched cluster', function (done) {
            var set = MeasurementSet.findSet(1, 1, 3000);
            var promise = set.fetchBetween(2000, 3000);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 2000,
                'endTime': 3000,
                'lastModified': 3000,
                'clusterCount': 2,
                'status': 'OK'});

            promise.then(function () {
                assert(!set.hasFetchedRange(2500, 3500));
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should return true when the range is within two fetched clusters', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            var promise = set.fetchBetween(2000, 3000);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': [],
                'configurations': {current: []},
                'startTime': 3000,
                'endTime': 4000,
                'lastModified': 5000,
                'clusterCount': 2,
                'status': 'OK'});

            waitForMeasurementSet().then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-3000.json');
                requests[1].resolve({
                    'clusterStart': 1000,
                    'clusterSize': 1000,
                    'formatMap': [],
                    'configurations': {current: []},
                    'startTime': 2000,
                    'endTime': 3000,
                    'lastModified': 5000,
                    'clusterCount': 2,
                    'status': 'OK'});                
            }).then(function () {
                assert(set.hasFetchedRange(2500, 3500));
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should return false when there is a cluster missing in the range', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            var promise = set.fetchBetween(2000, 5000);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

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

            waitForMeasurementSet().then(function () {
                assert.equal(requests.length, 3);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-3000.json');
                assert.equal(requests[2].url, '../data/measurement-set-1-1-4000.json');
                requests[1].resolve({
                    'clusterStart': 1000,
                    'clusterSize': 1000,
                    'formatMap': [],
                    'configurations': {current: []},
                    'startTime': 2000,
                    'endTime': 3000,
                    'lastModified': 5000,
                    'clusterCount': 2,
                    'status': 'OK'});
            }).then(function () {
                assert(!set.hasFetchedRange(2500, 4500));
                assert(set.hasFetchedRange(2100, 2300));
                assert(set.hasFetchedRange(4000, 4800));
                done();
            }).catch(function (error) {
                done(error);
            });
        });

    });

    describe('fetchedTimeSeries', () => {

        // Data from https://perf.webkit.org/v3/#/charts?paneList=((15-769))&since=1476426488465
        const sampleCluster = {
            "clusterStart": 946684800000,
            "clusterSize": 5184000000,
            "configurations": {
                "current": [
                    [
                        26530031, 135.26375, 80, 10821.1, 1481628.13, false,
                        [ [27173, 1, "210096", 1482398562950] ],
                        1482398562950, 52999, 1482413222311, "10877", 7
                    ],
                    [
                        26530779, 153.2675, 80, 12261.4, 1991987.4, true, // changed to true.
                        [ [27174,1,"210097",1482424870729] ],
                        1482424870729, 53000, 1482424992735, "10878", 7
                    ],
                    [
                        26532275, 134.2725, 80, 10741.8, 1458311.88, false,
                        [ [ 27176, 1, "210102", 1482431464371 ] ],
                        1482431464371, 53002, 1482436041865, "10879", 7
                    ],
                    [
                        26547226, 150.9625, 80, 12077, 1908614.94, false,
                        [ [ 27195, 1, "210168", 1482852412735 ] ],
                        1482852412735, 53022, 1482852452143, "10902", 7
                    ],
                    [
                        26559915, 141.72, 80, 11337.6, 1633126.8, false,
                        [ [ 27211, 1, "210222", 1483347732051 ] ],
                        1483347732051, 53039, 1483347926429, "10924", 7
                    ],
                    [
                        26564388, 138.13125, 80, 11050.5, 1551157.93, false,
                        [ [ 27217, 1, "210231", 1483412171531 ] ],
                        1483412171531, 53045, 1483415426049, "10930", 7
                    ],
                    [
                        26568867, 144.16, 80, 11532.8, 1694941.1, false,
                        [ [ 27222, 1, "210240", 1483469584347 ] ],
                        1483469584347, 53051, 1483469642993, "10935", 7
                    ]
                ]
            },
            "formatMap": [
                "id", "mean", "iterationCount", "sum", "squareSum", "markedOutlier",
                "revisions",
                "commitTime", "build", "buildTime", "buildNumber", "builder"
            ],
            "startTime": 1480636800000,
            "endTime": 1485820800000,
            "lastModified": 1484105738736,
            "clusterCount": 1,
            "elapsedTime": 56.421995162964,
            "status": "OK"
        };

        let builder;
        let webkit;
        beforeEach(function () {
            builder = new Builder(7, {name: 'EFL Linux 64-bit Release WK2 (Perf)', buildUrl: 'http://build.webkit.org/builders/$builderName/$buildNumber'});
            webkit = new Repository(1, {name: 'WebKit', url: 'http://trac.webkit.org/changeset/$1'});
        });

        function fetchSampleCluster() {
            const set = MeasurementSet.findSet(15, 769, 1484105738736);
            const promise = set.fetchBetween(1476426488465, 1484203801573);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-15-769.json');

            requests[0].resolve(sampleCluster);

            return waitForMeasurementSet().then(() => { return set; })
        }

        it('should include all data points from the fetched cluster when includeOutliers is true', () => {
            let points;
            return fetchSampleCluster().then((set) => {
                const includeOutliers = true;
                const extendToFuture = false;
                const fullSeries = set.fetchedTimeSeries('current', includeOutliers, extendToFuture);
                assert.equal(fullSeries.length(), 7);

                points = new Array(7);
                for (let i = 0; i < 7; i++)
                    points[i] = fullSeries.findPointByIndex(i);
            }).then(() => {
                const point = points[0];
                assert.equal(point.id, 26530031);

                const commitTime = 1482398562950;
                const rootSet = point.rootSet();
                assert.equal(point.rootSet(), rootSet);
                assert.deepEqual(rootSet.repositories(), [webkit]);
                assert.equal(rootSet.revisionForRepository(webkit), '210096');
                const commit = rootSet.commitForRepository(webkit);
                assert.equal(commit.repository(), webkit);
                assert.equal(+commit.time(), commitTime);
                assert.equal(commit.author(), null);
                assert.equal(commit.revision(), '210096');
                assert.equal(commit.message(), null);
                assert.equal(commit.url(), 'http://trac.webkit.org/changeset/210096');
                assert.equal(commit.label(), 'r210096');
                assert.equal(commit.title(), 'WebKit at r210096');
                assert.equal(rootSet.latestCommitTime(), commitTime);

                const build = point.build();
                assert.equal(point.build(), build);
                assert.equal(build.builder(), builder);
                assert.equal(build.buildNumber(), 10877);
                const buildTime = build.buildTime();
                assert(buildTime instanceof Date);
                assert.equal(+buildTime, 1482413222311);

                assert.equal(point.time, commitTime);
                assert.equal(point.value, 135.26375);
                assert.equal(point.sum, 10821.1);
                assert.equal(point.squareSum, 1481628.13);
                assert.equal(point.iterationCount, 80);
                assert.equal(point.markedOutlier, false);
            }).then(() => {
                const point = points[1];
                assert.equal(point.id, 26530779);

                const commitTime = 1482424870729;
                const rootSet = point.rootSet();
                assert.equal(rootSet.revisionForRepository(webkit), '210097');
                const commit = rootSet.commitForRepository(webkit);
                assert.equal(+commit.time(), commitTime);
                assert.equal(commit.revision(), '210097');
                assert.equal(rootSet.latestCommitTime(), commitTime);

                const build = point.build();
                assert.equal(build.builder(), builder);
                assert.equal(build.buildNumber(), 10878);
                assert.equal(+build.buildTime(), 1482424992735);

                assert.equal(point.time, commitTime);
                assert.equal(point.value, 153.2675);
                assert.equal(point.sum, 12261.4);
                assert.equal(point.squareSum, 1991987.4);
                assert.equal(point.iterationCount, 80);
                assert.equal(point.markedOutlier, true);
            }).then(() => {
                assert.equal(points[2].id, 26532275);
                assert.equal(points[2].rootSet().revisionForRepository(webkit), '210102');
                assert.equal(+points[2].build().buildTime(), 1482436041865);
                assert.equal(points[2].build().buildNumber(), 10879);
                assert.equal(points[2].time, 1482431464371);
                assert.equal(points[2].value, 134.2725);

                assert.equal(points[3].id, 26547226);
                assert.equal(points[3].rootSet().revisionForRepository(webkit), '210168');
                assert.equal(+points[3].build().buildTime(), 1482852452143);
                assert.equal(points[3].build().buildNumber(), 10902);
                assert.equal(points[3].time, 1482852412735);
                assert.equal(points[3].value, 150.9625);

                assert.equal(points[4].id, 26559915);
                assert.equal(points[4].rootSet().revisionForRepository(webkit), '210222');
                assert.equal(+points[4].build().buildTime(), 1483347926429);
                assert.equal(points[4].build().buildNumber(), 10924);
                assert.equal(points[4].time, 1483347732051);
                assert.equal(points[4].value, 141.72);

                assert.equal(points[5].id, 26564388);
                assert.equal(points[5].rootSet().revisionForRepository(webkit), '210231');
                assert.equal(+points[5].build().buildTime(), 1483415426049);
                assert.equal(points[5].build().buildNumber(), 10930);
                assert.equal(points[5].time, 1483412171531);
                assert.equal(points[5].value, 138.13125);

                assert.equal(points[6].id, 26568867);
                assert.equal(points[6].rootSet().revisionForRepository(webkit), '210240');
                assert.equal(+points[6].build().buildTime(), 1483469642993);
                assert.equal(points[6].build().buildNumber(), 10935);
                assert.equal(points[6].time, 1483469584347);
                assert.equal(points[6].value, 144.16);
            });
        });

        it('should include all data points from the fetched cluster when includeOutliers is false', () => {
            return fetchSampleCluster().then((set) => {
                const includeOutliers = false;
                const extendToFuture = false;
                const fullSeries = set.fetchedTimeSeries('current', includeOutliers, extendToFuture);
                assert.equal(fullSeries.length(), 6);

                const points = new Array(6);
                for (let i = 0; i < 6; i++)
                    points[i] = fullSeries.findPointByIndex(i);

                assert.equal(points[0].id, 26530031);
                assert.equal(points[0].rootSet().revisionForRepository(webkit), '210096');
                assert.equal(+points[0].build().buildTime(), 1482413222311);
                assert.equal(points[0].build().buildNumber(), 10877);
                assert.equal(points[0].time, 1482398562950);
                assert.equal(points[0].value, 135.26375);

                assert.equal(points[1].id, 26532275);
                assert.equal(points[1].rootSet().revisionForRepository(webkit), '210102');
                assert.equal(+points[1].build().buildTime(), 1482436041865);
                assert.equal(points[1].build().buildNumber(), 10879);
                assert.equal(points[1].time, 1482431464371);
                assert.equal(points[1].value, 134.2725);

                assert.equal(points[2].id, 26547226);
                assert.equal(points[2].rootSet().revisionForRepository(webkit), '210168');
                assert.equal(+points[2].build().buildTime(), 1482852452143);
                assert.equal(points[2].build().buildNumber(), 10902);
                assert.equal(points[2].time, 1482852412735);
                assert.equal(points[2].value, 150.9625);

                assert.equal(points[3].id, 26559915);
                assert.equal(points[3].rootSet().revisionForRepository(webkit), '210222');
                assert.equal(+points[3].build().buildTime(), 1483347926429);
                assert.equal(points[3].build().buildNumber(), 10924);
                assert.equal(points[3].time, 1483347732051);
                assert.equal(points[3].value, 141.72);

                assert.equal(points[4].id, 26564388);
                assert.equal(points[4].rootSet().revisionForRepository(webkit), '210231');
                assert.equal(+points[4].build().buildTime(), 1483415426049);
                assert.equal(points[4].build().buildNumber(), 10930);
                assert.equal(points[4].time, 1483412171531);
                assert.equal(points[4].value, 138.13125);

                assert.equal(points[5].id, 26568867);
                assert.equal(points[5].rootSet().revisionForRepository(webkit), '210240');
                assert.equal(+points[5].build().buildTime(), 1483469642993);
                assert.equal(points[5].build().buildNumber(), 10935);
                assert.equal(points[5].time, 1483469584347);
                assert.equal(points[5].value, 144.16);
            });
        });

    });

    describe('fetchSegmentation', function () {

        var simpleSegmentableValues = [
            1546.5603, 1548.1536, 1563.5452, 1539.7823, 1546.4184, 1548.9299, 1532.5444, 1546.2800, 1547.1760, 1551.3507,
            1548.3277, 1544.7673, 1542.7157, 1538.1700, 1538.0948, 1543.0364, 1537.9737, 1542.2611, 1543.9685, 1546.4901,
            1544.4080, 1540.8671, 1537.3353, 1549.4331, 1541.4436, 1544.1299, 1550.1770, 1553.1872, 1549.3417, 1542.3788,
            1543.5094, 1541.7905, 1537.6625, 1547.3840, 1538.5185, 1549.6764, 1556.6138, 1552.0476, 1541.7629, 1544.7006,
            /* segments changes here */
            1587.1390, 1594.5451, 1586.2430, 1596.7310, 1548.1423
        ];

        function makeSampleRuns(values, startRunId, startTime, timeIncrement)
        {
            var runId = startRunId;
            var buildId = 3400;
            var buildNumber = 1;
            var makeRun = function (value, commitTime) {
                return [runId++, value, 1, value, value, false, [], commitTime, commitTime + 10, buildId++, buildNumber++, MockModels.builder.id()];
            }

            timeIncrement = Math.floor(timeIncrement);
            var runs = values.map(function (value, index) { return makeRun(value, startTime + index * timeIncrement); })
            
            return runs;
        }

        it('should be able to segment a single cluster', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            var promise = set.fetchBetween(4000, 5000);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

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

            var timeSeries;
            assert.equal(set.fetchedTimeSeries('current', false, false).length(), 0);
            waitForMeasurementSet().then(function () {
                timeSeries = set.fetchedTimeSeries('current', false, false);
                assert.equal(timeSeries.length(), 45);
                assert.equal(timeSeries.firstPoint().time, 4000);
                assert.equal(timeSeries.lastPoint().time, 4880);
                return set.fetchSegmentation('segmentTimeSeriesByMaximizingSchwarzCriterion', [], 'current', false);
            }).then(function (segmentation) {
                assert.equal(segmentation.length, 4);

                assert.equal(segmentation[0].time, 4000);
                assert.almostEqual(segmentation[0].value, 1545.082);
                assert.equal(segmentation[0].value, segmentation[1].value);
                assert.equal(segmentation[1].time, timeSeries.findPointByIndex(39).time);

                assert.equal(segmentation[2].time, timeSeries.findPointByIndex(39).time);
                assert.almostEqual(segmentation[2].value, 1581.872);
                assert.equal(segmentation[2].value, segmentation[3].value);
                assert.equal(segmentation[3].time, 4880);
                done();
            }).catch(done);
        });

        it('should be able to segment two clusters', function (done) {
            var set = MeasurementSet.findSet(1, 1, 5000);
            var promise = set.fetchBetween(3000, 5000);
            assert.equal(requests.length, 1);
            assert.equal(requests[0].url, '../data/measurement-set-1-1.json');

            requests[0].resolve({
                'clusterStart': 1000,
                'clusterSize': 1000,
                'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                'configurations': {current: makeSampleRuns(simpleSegmentableValues.slice(30), 6400, 4000, 1000 / 30)},
                'startTime': 4000,
                'endTime': 5000,
                'lastModified': 5000,
                'clusterCount': 4,
                'status': 'OK'});

            waitForMeasurementSet().then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-4000.json');
                return set.fetchSegmentation('segmentTimeSeriesByMaximizingSchwarzCriterion', [], 'current', false);
            }).then(function (segmentation) {
                var timeSeries = set.fetchedTimeSeries('current', false, false);
                assert.equal(timeSeries.length(), 15);
                assert.equal(timeSeries.firstPoint().time, 4000);
                assert.equal(timeSeries.lastPoint().time, 4462);

                assert.equal(segmentation.length, 4);
                assert.equal(segmentation[0].time, timeSeries.firstPoint().time);
                assert.almostEqual(segmentation[0].value, 1545.441);
                assert.equal(segmentation[0].value, segmentation[1].value);
                assert.equal(segmentation[1].time, timeSeries.findPointByIndex(9).time);

                assert.equal(segmentation[2].time, timeSeries.findPointByIndex(9).time);
                assert.almostEqual(segmentation[2].value, 1581.872);
                assert.equal(segmentation[2].value, segmentation[3].value);
                assert.equal(segmentation[3].time, timeSeries.lastPoint().time);

                requests[1].resolve({
                    'clusterStart': 1000,
                    'clusterSize': 1000,
                    'formatMap': ['id', 'mean', 'iterationCount', 'sum', 'squareSum', 'markedOutlier', 'revisions', 'commitTime', 'build', 'buildTime', 'buildNumber', 'builder'],
                    'configurations': {current: makeSampleRuns(simpleSegmentableValues.slice(0, 30), 6500, 3000, 1000 / 30)},
                    'startTime': 3000,
                    'endTime': 4000,
                    'lastModified': 5000,
                    'clusterCount': 4,
                    'status': 'OK'});
                return waitForMeasurementSet();
            }).then(function () {
                return set.fetchSegmentation('segmentTimeSeriesByMaximizingSchwarzCriterion', [], 'current', false);
            }).then(function (segmentation) {
                var timeSeries = set.fetchedTimeSeries('current', false, false);
                assert.equal(timeSeries.length(), 45);
                assert.equal(timeSeries.firstPoint().time, 3000);
                assert.equal(timeSeries.lastPoint().time, 4462);
                assert.equal(segmentation.length, 4);

                assert.equal(segmentation[0].time, timeSeries.firstPoint().time);
                assert.almostEqual(segmentation[0].value, 1545.082);
                assert.equal(segmentation[0].value, segmentation[1].value);
                assert.equal(segmentation[1].time, timeSeries.findPointByIndex(39).time);

                assert.equal(segmentation[2].time, timeSeries.findPointByIndex(39).time);
                assert.almostEqual(segmentation[2].value, 1581.872);
                assert.equal(segmentation[2].value, segmentation[3].value);
                assert.equal(segmentation[3].time, timeSeries.lastPoint().time);
                done();
            }).catch(done);
        });

    });
});
