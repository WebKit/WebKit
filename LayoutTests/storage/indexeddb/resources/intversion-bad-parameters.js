if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that bad version parameters cause TypeError");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    preamble();
    evalAndExpectExceptionClass("indexedDB.open(dbname, 'stringversion')", "TypeError");
    evalAndExpectExceptionClass("indexedDB.open(dbname, 0)", "TypeError");
    evalAndExpectExceptionClass("indexedDB.open(dbname, -5)", "TypeError");
    debug("");
    debug("FIXME: Using -1 doesn't throw TypeError but it should");
    evalAndExpectExceptionClass("request = indexedDB.open(dbname, -1)", "TypeError");
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
}

test();
