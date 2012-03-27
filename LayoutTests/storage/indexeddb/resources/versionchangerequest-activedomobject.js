if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
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

    // NOTE: This is just an IDBRequest, it can be global.
    evalAndLog("openRequest = indexedDB.open(self.dbname)");
    openRequest.onerror = unexpectedErrorCallback;
    openRequest.onsuccess = function () {
        evalAndLog("db = openRequest.result");

        // NOTE: versionRequest is local variable so it is not captured in global scope.
        var versionRequest = evalAndLog("db.setVersion('1')");
        versionRequest.onerror = unexpectedErrorCallback;
        versionRequest.onblocked = unexpectedBlockedCallback;
        versionRequest.onsuccess = function() { versionSuccess(); };
        versionRequest = null;
        if (self.gc) {
            evalAndLog("self.gc()");
        }
    };
}

function versionSuccess()
{
    testPassed("setVersion's IDBVersionChangeRequest.onsuccess event fired");
    finishJSTest();
}

test();