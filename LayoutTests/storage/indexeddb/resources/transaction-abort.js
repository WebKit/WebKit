if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test transaction aborts send the proper onabort messages..");

indexedDBTest(prepareDatabase, startTest);
function prepareDatabase()
{
    db = event.target.result;
    store = evalAndLog("store = db.createObjectStore('storeName', null)");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onerror = unexpectedErrorCallback;
}

function startTest()
{
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = transactionAborted");
    evalAndLog("trans.oncomplete = unexpectedCompleteCallback");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value2', y: 'zzz2'}, 'key2')");
    request.onerror = firstAdd;
    request.onsuccess = unexpectedSuccessCallback;
    request = evalAndLog("store.add({x: 'value3', y: 'zzz3'}, 'key3')");
    request.onerror = secondAdd;
    request.onsuccess = unexpectedSuccessCallback;
    trans.abort();

    firstError = false;
    secondError = false;
    abortFired = false;
}

function firstAdd()
{
    shouldBe("event.target.error.name", "'AbortError'");
    shouldBeNull("trans.error");
    shouldBeFalse("firstError");
    shouldBeFalse("secondError");
    shouldBeFalse("abortFired");
    firstError = true;

    evalAndExpectException("store.add({x: 'value4', y: 'zzz4'}, 'key4')", "0", "'TransactionInactiveError'");
}

function secondAdd()
{
    shouldBe("event.target.error.name", "'AbortError'");
    shouldBeNull("trans.error");
    shouldBeTrue("firstError");
    shouldBeFalse("secondError");
    shouldBeFalse("abortFired");
    secondError = true;
}

function transactionAborted()
{
    shouldBeTrue("firstError");
    shouldBeTrue("secondError");
    shouldBeFalse("abortFired");
    shouldBeNull("trans.error");
    abortFired = true;

    evalAndExpectException("store.add({x: 'value5', y: 'zzz5'}, 'key5')", "0", "'TransactionInactiveError'");

    evalAndExpectException("trans.abort()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    finishJSTest();
}
