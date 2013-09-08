if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's basics.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('basics')");
    shouldBeTrue("'result' in request");
    evalAndExpectException("request.result", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    shouldBeTrue("'webkitErrorMessage' in request");
    evalAndExpectException("request.webkitErrorMessage", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("request.error", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    shouldBeTrue("'source' in request");
    shouldBeNull("request.source");
    shouldBeTrue("'transaction' in request");
    shouldBeNull("request.transaction");
    shouldBeTrue("'readyState' in request");
    shouldBeEqualToString("request.readyState", "pending");
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
    shouldBeTrue("'webkitErrorMessage' in event.target");
    shouldBeUndefined("event.target.webkitErrorMessage");
    shouldBeTrue("'error' in event.target");
    shouldBeNull("event.target.error");
    shouldBeTrue("'source' in event.target");
    shouldBeNull("request.source");
    shouldBeTrue("'transaction' in event.target");
    shouldBeNull("event.target.transaction");
    shouldBeTrue("'readyState' in request");
    shouldBeEqualToString("event.target.readyState", "done");
    shouldBeTrue("'onsuccess' in event.target");
    shouldBeTrue("'onerror' in event.target");

    finishJSTest();
}

test();
