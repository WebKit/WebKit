description("This test aborts the same transaction twice, making the appropriate exception is thrown.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("DoubleAbortTestDatabase", 1);

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
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
