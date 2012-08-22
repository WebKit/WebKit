if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Checks that garbage collection doesn't reclaim objects with pending activity");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();
    prepareDatabase();
}

function prepareDatabase()
{
    preamble();
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        request = evalAndLog("indexedDB.open(dbname)");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(e) {
            evalAndLog("db = request.result");
            shouldBeEqualToString("db.version", "");
            request = evalAndLog("db.setVersion('1')");
            request.onerror = unexpectedErrorCallback;
            request.onsuccess = function(e) {
                trans = request.result;
                evalAndLog("store = db.createObjectStore('store')");
                evalAndLog("store.put(0, 0)");
                trans.oncomplete = testTransaction;
            };
        };
    };
}

function testTransaction()
{
    preamble();
    evalAndLog("transaction = db.transaction('store')");
    evalAndLog("transaction.oncomplete = transactionOnComplete");
    evalAndLog("transaction = null");
    evalAndLog("self.gc()");
}

function transactionOnComplete()
{
    testPassed("Transaction 'complete' event fired.");
    testRequest();
}

function testRequest()
{
    preamble();
    evalAndLog("transaction = db.transaction('store')");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("request = store.get(0)");
    evalAndLog("request.onsuccess = requestOnSuccess");
    evalAndLog("request = null");
    evalAndLog("self.gc()");
}

function requestOnSuccess()
{
    testPassed("Request 'success' event fired.");
    testCursorRequest();
}

function testCursorRequest()
{
    preamble();
    evalAndLog("transaction = db.transaction('store')");
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("request = store.openCursor()");
    evalAndLog("request.onsuccess = cursorRequestOnFirstSuccess");
}

function cursorRequestOnFirstSuccess()
{
    testPassed("Request 'success' event fired.");
    evalAndLog("cursor = request.result");
    evalAndLog("cursor.continue()");
    evalAndLog("request.onsuccess = cursorRequestOnSecondSuccess");
    evalAndLog("request = null");
    evalAndLog("self.gc()");
}

function cursorRequestOnSecondSuccess()
{
    testPassed("Request 'success' event fired.");

    debug("");
    finishJSTest();
}

test();
