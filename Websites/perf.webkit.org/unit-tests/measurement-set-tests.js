'use strict';

var assert = require('assert');

require('./resources/mock-remote-api.js');
require('../tools/js/v3-models.js');
require('./resources/mock-v3-models.js');

describe('MeasurementSet', function () {
    beforeEach(function () {
        MeasurementSet._set = null;
    });

    describe('findSet', function () {
        it('should create a new MeasurementSet for a new pair of platform and matric', function () {
            assert.notEqual(MeasurementSet.findSet(1, 1, 3000), MeasurementSet.findSet(1, 2, 3000));
            assert.notEqual(MeasurementSet.findSet(1, 1, 3000), MeasurementSet.findSet(2, 1, 3000));
        });

        it('should not create a new MeasurementSet when the same pair of platform and matric are requested', function () {
            assert.equal(MeasurementSet.findSet(1, 1), MeasurementSet.findSet(1, 1));
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
            set.fetchBetween(2000, 3000, function () {
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

            requests[0].promise.then(function () {
                assert.equal(callCount, 1);
                assert.equal(requests.length, 1);
                done();
            }).catch(function (error) {
                done(error);
            });
        });

        it('should invoke the callback and fetch a secondary cluster'
            + 'when the cached primary cluster is up-to-date and within in the requested range', function (done) {
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

            Promise.resolve().then(function () {
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
                'clusterCount': 3,
                'status': 'OK'});

            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-3000.json');

                var callCount = 0;
                set.fetchBetween(0, 7000, function () { callCount++; });

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

            Promise.resolve().then(function () {
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
                'clusterCount': 3,
                'status': 'OK'});

            Promise.resolve().then(function () {
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-4000.json');

                var callCount = 0;
                set.fetchBetween(1207, 1293, function () { callCount++; });

                assert.equal(callCount, 0);
                assert.equal(requests.length, 3);
                assert.equal(requests[2].url, '../data/measurement-set-1-1-2000.json');

                set.fetchBetween(1964, 3401, function () { callCount++; });

                assert.equal(callCount, 0);
                assert.equal(requests.length, 4);
                assert.equal(requests[3].url, '../data/measurement-set-1-1-3000.json');

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

            Promise.resolve().then(function () {
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

            Promise.resolve().then(function () {
                assert.equal(callCount, 0);
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../api/measurement-set?platform=1&metric=1');
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

            Promise.resolve().then(function () {
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
                'clusterCount': 3,
                'status': 'OK'});

            var callCountFor4000To5000 = 0;
            var callCountFor2000 = 0;
            var callCountFor2000To4000 = 0;
            Promise.resolve().then(function () {
                assert.equal(callCountFor4000, 0);
                assert.equal(requests.length, 2);
                assert.equal(requests[1].url, '../data/measurement-set-1-1-4000.json');

                set.fetchBetween(3708, 4800, function () { callCountFor4000To5000++; });
                assert.equal(callCountFor4000To5000, 1);
                assert.equal(requests.length, 2);

                set.fetchBetween(1207, 1293, function () { callCountFor2000++; });
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
            }).then(function () {
                assert.equal(requests.length, 3);
                assert.equal(callCountFor4000, 0);
                assert.equal(callCountFor4000To5000, 1);
                assert.equal(callCountFor2000, 1);

                set.fetchBetween(1964, 3401, function () { callCountFor2000To4000++; });

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

});