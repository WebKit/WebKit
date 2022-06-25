if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure deleteDatabase() can run concurrently with transactions in other databases");

indexedDBTest(prepareDatabase, startTransaction);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("db.createObjectStore('store')");
}

function startTransaction() {
    debug("");
    debug("Start a transaction against the first database:");
    evalAndLog("trans = db.transaction('store', 'readonly')");
    evalAndLog("trans.objectStore('store').get(0)");

    debug("");
    debug("Delete a different database:");
    evalAndLog("dbname2 = dbname + '2'");
    request = evalAndLog("indexedDB.deleteDatabase(dbname2)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        testPassed("success event was fired at delete request");
    };

    trans.oncomplete = finishJSTest;
}
