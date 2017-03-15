'use strict';

const assert = require('assert');

require('../tools/js/v3-models.js');

const TestServer = require('./resources/test-server.js');
const MockData = require('./resources/mock-data.js');
const addSlaveForReport = require('./resources/common-operations.js').addSlaveForReport;
const prepareServerTest = require('./resources/common-operations.js').prepareServerTest;

describe('/api/update-triggerable/', function () {
    prepareServerTest(this);

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

    it('should reject when slave name is missing', () => {
        return TestServer.remoteAPI().postJSON('/api/update-triggerable/', {}).then((response) => {
            assert.equal(response['status'], 'MissingSlaveName');
        });
    });

    it('should reject when there are no slaves', () => {
        const update = {slaveName: emptyUpdate.slaveName, slavePassword: emptyUpdate.slavePassword};
        return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update).then((response) => {
            assert.equal(response['status'], 'SlaveNotFound');
        });
    });

    it('should reject when the slave password doesn\'t match', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addSlaveForReport(emptyUpdate);
        }).then(() => {
            const report = {slaveName: emptyUpdate.slaveName, slavePassword: 'badPassword'};
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
        });
    });

    it('should accept an empty report', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addSlaveForReport(emptyUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
        });
    });

    it('delete existing configurations when accepting an empty report', () => {
        const db = TestServer.database();
        return MockData.addMockData(db).then(() => {
            return Promise.all([
                addSlaveForReport(emptyUpdate),
                db.insert('triggerable_configurations',
                    {'triggerable': 1 /* build-webkit */, 'test': MockData.someTestId(), 'platform': MockData.somePlatformId()})
            ]);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', emptyUpdate);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('triggerable_configurations', 'test');
        }).then((rows) => {
            assert.equal(rows.length, 0);
        });
    });

    it('should add configurations in the update', () => {
        const db = TestServer.database();
        return MockData.addMockData(db).then(() => {
            return addSlaveForReport(smallUpdate);
        }).then(() => {
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', smallUpdate);
        }).then((response) => {
            assert.equal(response['status'], 'OK');
            return db.selectAll('triggerable_configurations', 'test');
        }).then((rows) => {
            assert.equal(rows.length, 1);
            assert.equal(rows[0]['test'], smallUpdate.configurations[0]['test']);
            assert.equal(rows[0]['platform'], smallUpdate.configurations[0]['platform']);
        });
    });

    it('should reject when a configuration is malformed', () => {
        return MockData.addMockData(TestServer.database()).then(() => {
            return addSlaveForReport(smallUpdate);
        }).then(() => {
            const update = {
                'slaveName': 'someSlave',
                'slavePassword': 'somePassword',
                'triggerable': 'build-webkit',
                'configurations': [{}],
            };
            return TestServer.remoteAPI().postJSON('/api/update-triggerable/', update);
        }).then((response) => {
            assert.equal(response['status'], 'InvalidConfigurationEntry');
        });
    });

});
