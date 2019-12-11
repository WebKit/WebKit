if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure that IDBVersionChangeRequest objects are not GC'd if they have pending events");

function test() {
    removeVendorPrefixes();

    debug("");
    evalAndLog("self.dbname = 'versionchangerequest-activedomobject'");

    testDeleteDatabase();
}

function testDeleteDatabase()
{
    debug("");
    debug("testDeleteDatabase():");
    // NOTE: deleteRequest is local variable so it is not captured in global scope.
    var deleteRequest = evalAndLog("indexedDB.deleteDatabase(self.dbname)");
    deleteRequest.onerror = unexpectedErrorCallback;
    deleteRequest.onblocked = unexpectedBlockedCallback;
    deleteRequest.onsuccess = function() { deleteSuccess(); };
    deleteRequest = null;
    if (self.gc) {
        evalAndLog("self.gc()");
    }
}

function deleteSuccess()
{
    testPassed("deleteDatabase's IDBVersionChangeRequest.onsuccess event fired");
    testSetVersion();
}

function testSetVersion()
{
    debug("");
    debug("testSetVersion():");

    // NOTE: openRequest is local variable so it is not captured in global
    // scope.
    var openRequest = evalAndLog("indexedDB.open(self.dbname)");
    openRequest.onerror = unexpectedErrorCallback;
    openRequest.onblocked = unexpectedBlockedCallback;
    openRequest.onupgradeneeded = upgradeNeededCallback;
    openRequest.onsuccess = successCallback;
    openRequest = null;
    if (self.gc) {
        evalAndLog("self.gc()");
    }
}

function upgradeNeededCallback()
{
    testPassed("IDBOpenDBRequest received upgradeneeded event");
}

function successCallback()
{
    testPassed("IDBOpenDBRequest received success event");
    finishJSTest();
}

test();
