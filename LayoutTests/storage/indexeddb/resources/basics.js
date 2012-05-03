if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's basics.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('basics')");
    shouldBeTrue("'result' in request");
    evalAndExpectException("request.result", "IDBDatabaseException.NOT_ALLOWED_ERR");
    shouldBeTrue("'errorCode' in request");
    evalAndExpectException("request.errorCode", "IDBDatabaseException.NOT_ALLOWED_ERR");
    shouldBeTrue("'webkitErrorMessage' in request");
    evalAndExpectException("request.webkitErrorMessage", "IDBDatabaseException.NOT_ALLOWED_ERR");
    shouldBeTrue("'source' in request");
    shouldBe("request.source", "indexedDB");
    shouldBeTrue("'transaction' in request");
    shouldBeNull("request.transaction");
    shouldBeTrue("'readyState' in request");
    shouldBe("request.readyState", "'pending'");
    shouldBeTrue("'onsuccess' in request");
    shouldBeNull("request.onsuccess");
    shouldBeTrue("'onerror' in request");
    shouldBeNull("request.onerror");
    request.onsuccess = openCallback;
    request.onerror = unexpectedErrorCallback;
}

function openCallback(evt)
{
    event = evt;
    shouldBeTrue("'result' in event.target");
    shouldBeTrue("!!event.target.result");
    shouldBeTrue("'errorCode' in event.target");
    shouldBe("event.target.errorCode", "0");
    shouldBeTrue("'webkitErrorMessage' in event.target");
    shouldBeUndefined("event.target.webkitErrorMessage");
    shouldBeTrue("'source' in event.target");
    shouldBe("request.source", "indexedDB");
    shouldBeTrue("'transaction' in event.target");
    shouldBeNull("event.target.transaction");
    shouldBeTrue("'readyState' in request");
    shouldBe("event.target.readyState", "'done'");
    shouldBeTrue("'onsuccess' in event.target");
    shouldBeTrue("'onerror' in event.target");

    finishJSTest();
}

test();
