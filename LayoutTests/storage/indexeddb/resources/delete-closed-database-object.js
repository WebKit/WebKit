if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Ensure that IDBDatabase objects are deleted when there are no retaining paths left");

indexedDBTest(prepareDatabase, openSuccess);
function prepareDatabase()
{
}

function openSuccess()
{
    db = event.target.result;
    evalAndLog("db.close()");

    debug("We can't specify a version here due to http://wkbug.com/102716");
    var openRequest = evalAndLog("indexedDB.open(dbname)"); // NOTE: No version specified.
    openRequest.onblocked = unexpectedBlockedCallback;
    openRequest.onupgradeneeded = unexpectedUpgradeNeededCallback;
    openRequest.onerror = unexpectedErrorCallback;
    openRequest.onsuccess = function() {
        debug("Dropping references to new connection.");
        // After leaving this function, there are no remaining references to the
        // db, so it should get deleted.
        setTimeout(setVersion, 2);
    };
}

function setVersion()
{
    evalAndLog("gc()");
    debug("Open request should not receive a blocked event:");
    var request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = finishJSTest;
}
