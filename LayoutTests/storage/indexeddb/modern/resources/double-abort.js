description("This test aborts the same transaction twice, making the appropriate exception is thrown.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    var database = event.target.result;

    versionTransaction.abort();
    try {
        versionTransaction.abort();
    } catch (e) {
        debug("Second abort failed: " + e);
    }

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction unexpected complete");
        done();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}
