const assert = require('assert');
const crypto = require('crypto');

function addBuilderForReport(report)
{
    return TestServer.database().insert('builders', {
        name: report.builderName,
        password_hash: crypto.createHash('sha256').update(report.builderPassword).digest('hex')
    });
}

function addSlaveForReport(report)
{
    return TestServer.database().insert('build_slaves', {
        name: report.slaveName,
        password_hash: crypto.createHash('sha256').update(report.slavePassword).digest('hex')
    });
}

function prepareServerTest(test, privilegedAPIType='browser')
{
    test.timeout(5000);
    TestServer.inject(privilegedAPIType);

    beforeEach(async function () {
        const database = TestServer.database();
        if (typeof(MockData) != 'undefined')
            MockData.resetV3Models();
        await database.connect({keepAlive: true});
        if (privilegedAPIType === 'browser')
            return;
        const entry = await TestServer.database().selectFirstRow('build_slaves', {name: 'test'});
        if (!entry)
            await addSlaveForReport({slaveName: 'test', slavePassword: 'password'});
    });

    afterEach(function () {
        TestServer.database().disconnect();
    });
}

function submitReport(report)
{
    return TestServer.database().insert('builders', {
        name: report[0].builderName,
        password_hash: crypto.createHash('sha256').update(report[0].builderPassword).digest('hex')
    }).then(function () {
        return TestServer.remoteAPI().postJSON('/api/report/', report);
    });
}

async function assertThrows(expectedError, testFunction)
{
    let thrownException = false;
    try {
            await testFunction()
    } catch(error) {
        thrownException = true;
        assert.equal(error, expectedError);
    }
    assert.ok(thrownException);
}

if (typeof module != 'undefined') {
    module.exports.addBuilderForReport = addBuilderForReport;
    module.exports.addSlaveForReport = addSlaveForReport;
    module.exports.prepareServerTest = prepareServerTest;
    module.exports.submitReport = submitReport;
    module.exports.assertThrows = assertThrows;
}
