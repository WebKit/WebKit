if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
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
    evalAndExpectExceptionClass("indexedDB.open(dbname, Infinity)", "TypeError");
    evalAndExpectExceptionClass("indexedDB.open(dbname, -Infinity)", "TypeError");
    evalAndExpectExceptionClass("indexedDB.open(dbname, NaN)", "TypeError");
    evalAndExpectExceptionClass("indexedDB.open(dbname, -1)", "TypeError");
    evalAndExpectExceptionClass("indexedDB.open(dbname, 0x20000000000000)", "TypeError");
    evalAndExpectExceptionClass("indexedDB.open(dbname, null)", "TypeError");
    evalAndExpectExceptionClass("indexedDB.open(dbname, undefined)", "TypeError");
    finishJSTest();
}

test();
