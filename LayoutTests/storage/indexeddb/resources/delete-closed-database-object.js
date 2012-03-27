if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Ensure that IDBDatabase objects are deleted when there are no retaining paths left");

function test() {
    removeVendorPrefixes();
    openDBConnection();
}

function openDBConnection()
{
    evalAndLog("self.state = 'starting'");
    request = evalAndLog("indexedDB.open('delete-closed-database-object')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    self.db = evalAndLog("db = event.target.result");

    // Open a second connection to the same database.
    var request2 = evalAndLog("indexedDB.open('delete-closed-database-object')");
    request2.onsuccess = openSuccess2;
    request2.onerror = unexpectedErrorCallback;
}

function openSuccess2()
{
    debug("Second connection successfully established.");
    // After leaving this function, there are no remaining references to the
    // second connection, so it should get deleted.
    setTimeout(setVersion, 2);
}

function setVersion()
{
    gc();
    debug("calling setVersion() - callback should run immediately");
    var versionChangeRequest = evalAndLog("db.setVersion('version 1')");
    versionChangeRequest.onerror = unexpectedErrorCallback;
    versionChangeRequest.onblocked = unexpectedBlockedCallback;
    versionChangeRequest.onsuccess = finishJSTest;
}

test();