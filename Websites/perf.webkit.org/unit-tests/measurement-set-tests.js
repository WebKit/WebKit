'use strict';

var assert = require('assert');
if (!assert.almostEqual)
    assert.almostEqual = require('./resources/almost-equal.js');

let MockRemoteAPI = require('./resources/mock-remote-api.js').MockRemoteAPI;
require('../tools/js/v3-models.js');
let MockModels = require('./resources/mock-v3-models.js').MockModels;

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
