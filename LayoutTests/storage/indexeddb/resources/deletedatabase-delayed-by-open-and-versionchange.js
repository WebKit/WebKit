if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that deleteDatabase is delayed if a VERSION_CHANGE transaction is running");

indexedDBTest(prepareDatabase, onOpenSuccess);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("versionChangeComplete = false");
    evalAndLog("h = event.target.result");
    evalAndLog("blockedCalled = false");
        
    h.onversionchange = function onVersionChange(evt) {
        preamble(evt);
        shouldBe("event.target.version", "1");
        shouldBe("event.oldVersion", "1");
        shouldBeNull("event.newVersion");
    };

    transaction = event.target.transaction;
    transaction.oncomplete = function transactionOnComplete(evt) {
        preamble(evt);
        evalAndLog("versionChangeComplete = true");
    };

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = function deleteDatabaseOnBlocked(evt) {
        eval("blockedCalled = true");
    };
    request.onsuccess = function deleteDatabaseOnSuccess(evt) {
        preamble(evt);
        shouldBeTrue("versionChangeComplete");
        finishJSTest();
    };

    // Make this upgrade transaction take longer so the deleteDatabase request will always have a chance to be blocked.
    evalAndLog("h.createObjectStore('testObjectStore').put('bar', 'foo')");
}

function onOpenSuccess(evt)
{
    preamble(evt);
    shouldBeTrue("blockedCalled");
    evalAndLog("h = event.target.result");
    evalAndLog("h.close()");
}