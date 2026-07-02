description("This test aborts the same transaction twice, making the appropriate exception is thrown.");

var openRequest = null;
indexedDBTest(prepareDatabase);

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    openRequest = event.target;
    var versionTransaction = event.target.transaction;
    var database = event.target.result;

    versionTransaction.abort();
    try {
        versionTransaction.abort();
    } catch (e) {
        debug("Second abort failed: " + e);
    }

    versionTransaction.onabort = function(event) {
        // Set event handler to null to avoid unexpected error message being printed.
        openRequest.onerror = null;
        endTestWithLog("Initial upgrade versionchange transaction aborted");
    }

    versionTransaction.oncomplete = function(event) {
        endTestWithLog("Initial upgrade versionchange transaction unexpected complete");
    }

    versionTransaction.onerror = function(event) {
        endTestWithLog("Initial upgrade versionchange transaction unexpected error" + event);
    }
}
