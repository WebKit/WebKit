if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that deleteDatabase is delayed if a VERSION_CHANGE transaction is running");

indexedDBTest(prepareDatabase, onOpenSuccess);
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("versionChangeComplete = false");
    evalAndLog("h = event.target.result");

    h.onversionchange = function onVersionChange(evt) {
        preamble(evt);
        debug("old = " + JSON.stringify(event.target.version));
        debug("new = " + JSON.stringify(event.version));
    };

    transaction = event.target.transaction;
    transaction.oncomplete = function transactionOnComplete(evt) {
        preamble(evt);
        evalAndLog("versionChangeComplete = true");
    };

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = function deleteDatabaseOnBlocked(evt) {
        preamble(evt);
    };
    request.onsuccess = function deleteDatabaseOnSuccess(evt) {
        preamble(evt);
        shouldBeTrue("versionChangeComplete");
        finishJSTest();
    };
}

function onOpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("h = event.target.result");
    evalAndLog("h.close()");
}