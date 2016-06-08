'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const TestServer = require('./resources/test-server.js');
const MockData = require('./resources/mock-data.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const connectToDatabaseInEveryTest = require('./resources/common-operations.js').connectToDatabaseInEveryTest;

describe('/api/update-triggerable/', function () {
    this.timeout(1000);
    TestServer.inject();
    connectToDatabaseInEveryTest();

    const emptyUpdate = {
        'slaveName': 'someSlave',
        'slavePassword': 'somePassword',
        'triggerable': 'build-webkit',
        'configurations': [],
    };

    const smallUpdate = {
        'slaveName': 'someSlave',
        'slavePassword': 'somePassword',
        'triggerable': 'build-webkit',
        'configurations': [
            {test: MockData.someTestId(), platform: MockData.somePlatformId()}
        ],
    };

    it('should reject when slave name is missing', function (done) {
        TestServer.remoteAPI().postJSON('/api/update-triggerable/', {}).then(function (response) {
            assert.equal(response['status'], 'MissingSlaveName');
            done();
        }).catch(done);
    });

    it('should reject when there are no slaves', function (done) {
        const update = {slaveName: emptyUpdate.slaveName, slavePassword: emptyUpdate.slavePassword};
        TestServer.remoteAPI().postJSON('/api/update-triggerable/', update).then(function (response) {
            assert.equal(response['status'], 'SlaveNotFound');
            done();
        }).catch(done);
    });

    it('should reject when the slave password doesn\'t match', function (done) {
        MockData.addMockData(TestServer.database()).then(function () {
            return addSlaveForReport(emptyUpdate);
        }).then(function () {
            const report = {slaveName: emptyUpdate.slaveName, slavePassword: 'badPassword'};
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            done();
        }).catch(done);
    });

    it('should accept an empty report', function (done) {
        MockData.addMockData(TestServer.database()).then(function () {
            return addSlaveForReport(emptyUpdate);
        }).then(function () {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            done();
        }).catch(done);
    });

    it('delete existing configurations when accepting an empty report', function (done) {
        const db = TestServer.database();
        MockData.addMockData(db).then(function () {
            return Promise.all([
                addSlaveForReport(emptyUpdate),
                db.insert('triggerable_configurations',
                    {'triggerable': 1 /* build-webkit */, 'test': MockData.someTestId(), 'platform': MockData.somePlatformId()})
            ]);
        }).then(function () {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('triggerable_configurations', 'test');
        }).then(function (rows) {
            assert.equal(rows.length, 0);
            done();
        }).catch(done);
    });

    it('should add configurations in the update', function (done) {
        const db = TestServer.database();
        MockData.addMockData(db).then(function () {
            return addSlaveForReport(smallUpdate);
        }).then(function () {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', smallUpdate);
        }).then(function (response) {
            assert.equal(response['status'], 'OK');
            return db.selectAll('triggerable_configurations', 'test');
        }).then(function (rows) {
            assert.equal(rows.length, 1);
            assert.equal(rows[0]['test'], smallUpdate.configurations[0]['test']);
            assert.equal(rows[0]['platform'], smallUpdate.configurations[0]['platform']);
            done();
        }).catch(done);
    });

    it('should reject when a configuration is malformed', function (done) {
        const db = TestServer.database();
        MockData.addMockData(db).then(function () {
            return addSlaveForReport(smallUpdate);
        }).then(function () {
            const update = {
                'slaveName': 'someSlave',
                'slavePassword': 'somePassword',
                'triggerable': 'build-webkit',
                'configurations': [{}],
            };
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then(function (response) {
            assert.equal(response['status'], 'InvalidConfigurationEntry');
            done();
        }).catch(done);
    });

});
