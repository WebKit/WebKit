if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('idb-worker-common.js');
    importScripts('shared.js');
}

description("Test IndexedDB's basics.");

function test()
{
    shouldBeTrue("'webkitIndexedDB' in self");
    shouldBeFalse("webkitIndexedDB == null");

    shouldBeTrue("'webkitIDBCursor' in self");
    shouldBeFalse("webkitIDBCursor == null");

    request = evalAndLog("webkitIndexedDB.open('basics')");
    shouldBeTrue("'result' in request");
    evalAndExpectException("request.result", "webkitIDBDatabaseException.NOT_ALLOWED_ERR");
    shouldBeTrue("'errorCode' in request");
    evalAndExpectException("request.errorCode", "webkitIDBDatabaseException.NOT_ALLOWED_ERR");
    shouldBeTrue("'webkitErrorMessage' in request");
    evalAndExpectException("request.webkitErrorMessage", "webkitIDBDatabaseException.NOT_ALLOWED_ERR");
    shouldBeTrue("'source' in request");
    shouldBe("request.source", "webkitIndexedDB");
    shouldBeTrue("'transaction' in request");
    shouldBeNull("request.transaction");
    shouldBeTrue("'readyState' in request");
    shouldBe("request.readyState", "webkitIDBRequest.LOADING");
    shouldBeTrue("'onsuccess' in request");
    shouldBeNull("request.onsuccess");
    shouldBeTrue("'onerror' in request");
    shouldBeNull("request.onerror");
    shouldBe("request.LOADING", "1");
    shouldBe("request.DONE", "2");
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
    shouldBe("request.source", "webkitIndexedDB");
    shouldBeTrue("'transaction' in event.target");
    shouldBeNull("event.target.transaction");
    shouldBeTrue("'readyState' in request");
    shouldBe("event.target.readyState", "webkitIDBRequest.DONE");
    shouldBeTrue("'onsuccess' in event.target");
    shouldBeTrue("'onerror' in event.target");
    shouldBe("event.target.LOADING", "1");
    shouldBe("event.target.DONE", "2");

    finishJSTest();
}

test();
