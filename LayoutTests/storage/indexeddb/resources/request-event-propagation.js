if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test event propogation on IDBRequest.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('request-event-propagation')");
    request.onsuccess = setVersion;
    request.onerror = unexpectedErrorCallback;
}

function setVersion()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = deleteExisting;
    request.onerror = unexpectedErrorCallback;
}

function deleteExisting()
{
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;
    evalAndLog("trans.oncomplete = startTest");

    deleteAllObjectStores(db);

    store = evalAndLog("store = db.createObjectStore('storeName', null)");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onerror = unexpectedErrorCallback;
}

function startTest()
{
    debug("Verify that handler fires and that not preventing default will result in an abort");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = transactionAborted");
    evalAndLog("trans.oncomplete = unexpectedCompleteCallback");
    evalAndLog("trans.onerror = allowDefault");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onsuccess = unexpectedSuccessCallback;
    handlerFired = false;
}

function allowDefault()
{
    testPassed("Event handler fired");
    debug("Doing nothing to prevent the default action...");
    handlerFired = true;
}

function transactionAborted()
{
    shouldBeTrue("handlerFired");
    debug("");
    debug("Verifing error");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = transactionAborted2");
    evalAndLog("trans.oncomplete = unexpectedAbortCallback");
    evalAndLog("trans.addEventListener('error', errorCaptureCallback, true)");
    evalAndLog("trans.addEventListener('error', errorBubbleCallback, false)");
    evalAndLog("trans.addEventListener('success', unexpectedSuccessCallback, true)");
    evalAndLog("trans.addEventListener('success', unexpectedSuccessCallback, false)");
    evalAndLog("db.addEventListener('error', dbErrorCaptureCallback, true)");
    evalAndLog("db.addEventListener('error', dbErrorBubbleCallback, false)");
    evalAndLog("db.addEventListener('success', unexpectedSuccessCallback, true)");
    evalAndLog("db.addEventListener('success', unexpectedSuccessCallback, false)");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = errorFiredCallback;
    dbCaptureFired = false;
    captureFired = false;
    requestFired = false;
    bubbleFired = false;
    dbBubbleFired = false;
}

function dbErrorCaptureCallback()
{
    debug("");
    debug("In IDBDatabase error capture");
    shouldBeFalse("dbCaptureFired");
    shouldBeFalse("captureFired");
    shouldBeFalse("requestFired");
    shouldBeFalse("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    shouldBe("event.target", "request");
    shouldBe("event.currentTarget", "db");
    dbCaptureFired = true;
}

function errorCaptureCallback()
{
    debug("");
    debug("In IDBTransaction error capture");
    shouldBeTrue("dbCaptureFired");
    shouldBeFalse("captureFired");
    shouldBeFalse("requestFired");
    shouldBeFalse("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    shouldBe("event.target", "request");
    shouldBe("event.currentTarget", "trans");
    captureFired = true;
}

function errorFiredCallback()
{
    debug("");
    debug("In IDBRequest handler");
    shouldBeTrue("dbCaptureFired");
    shouldBeTrue("captureFired");
    shouldBeFalse("requestFired");
    shouldBeFalse("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    shouldBe("event.target", "request");
    shouldBe("event.currentTarget", "request");
    requestFired = true;
}

function errorBubbleCallback()
{
    debug("");
    debug("In IDBTransaction error bubble");
    shouldBeTrue("dbCaptureFired");
    shouldBeTrue("captureFired");
    shouldBeTrue("requestFired");
    shouldBeFalse("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    shouldBe("event.target", "request");
    shouldBe("event.currentTarget", "trans");
    bubbleFired = true;
}

function dbErrorBubbleCallback()
{
    debug("");
    debug("In IDBDatabase error bubble");
    shouldBeTrue("dbCaptureFired");
    shouldBeTrue("captureFired");
    shouldBeTrue("requestFired");
    shouldBeTrue("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    shouldBe("event.target", "request");
    shouldBe("event.currentTarget", "db");
    dbBubbleFired = true;
}

function transactionAborted2()
{
    debug("");
    debug("Transaction aborted");
    shouldBeTrue("dbCaptureFired");
    shouldBeTrue("captureFired");
    shouldBeTrue("requestFired");
    shouldBeTrue("bubbleFired");
    shouldBeTrue("dbBubbleFired");
    debug("");
    debug("Verifing success.");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.oncomplete = transactionComplete");
    evalAndLog("trans.onabort = unexpectedAbortCallback");
    evalAndLog("trans.addEventListener('success', successCaptureCallback, true)");
    evalAndLog("trans.addEventListener('success', successBubbleCallback, false)");
    evalAndLog("trans.addEventListener('error', unexpectedErrorCallback, true)");
    evalAndLog("trans.addEventListener('error', unexpectedErrorCallback, false)");
    evalAndLog("db.removeEventListener('error', dbErrorCaptureCallback, true)");
    evalAndLog("db.removeEventListener('error', dbErrorBubbleCallback, false)");
    evalAndLog("db.removeEventListener('success', unexpectedSuccessCallback, true)");
    evalAndLog("db.removeEventListener('success', unexpectedSuccessCallback, false)");
    evalAndLog("db.addEventListener('success', dbSuccessCaptureCallback, true)");
    evalAndLog("db.addEventListener('success', dbSuccessBubbleCallback, false)");
    evalAndLog("db.addEventListener('error', unexpectedErrorCallback, true)");
    evalAndLog("db.addEventListener('error', unexpectedErrorCallback, false)");
    store = evalAndLog("store = trans.objectStore('storeName')");
    request = evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key2')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = successFiredCallback;
    dbCaptureFired = false;
    captureFired = false;
    requestFired = false;
    bubbleFired = false;
    dbBubbleFired = false;
}

function dbSuccessCaptureCallback()
{
    debug("");
    debug("In IDBDatabase success capture");
    shouldBeFalse("dbCaptureFired");
    shouldBeFalse("captureFired");
    shouldBeFalse("requestFired");
    shouldBeFalse("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    shouldBe("event.target", "request");
    shouldBe("event.currentTarget", "db");
    dbCaptureFired = true;
}

function successCaptureCallback()
{
    debug("");
    debug("In IDBTransaction success capture");
    shouldBeTrue("dbCaptureFired");
    shouldBeFalse("captureFired");
    shouldBeFalse("requestFired");
    shouldBeFalse("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    shouldBe("event.target", "request");
    shouldBe("event.currentTarget", "trans");
    captureFired = true;
}

function successFiredCallback()
{
    debug("");
    debug("In IDBRequest handler");
    shouldBeTrue("dbCaptureFired");
    shouldBeTrue("captureFired");
    shouldBeFalse("requestFired");
    shouldBeFalse("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    shouldBe("event.target", "request");
    shouldBe("event.currentTarget", "request");
    requestFired = true;
}

function successBubbleCallback()
{
    debug("");
    debug("In IDBTransaction success bubble");
    testFailed("Success bubble callback should NOT fire");
    bubbleFired = true;
}

function dbSuccessBubbleCallback()
{
    debug("");
    debug("In IDBDatabase success bubble");
    testFailed("Success bubble callback should NOT fire");
    dbBubbleFired = true;
}

function transactionComplete()
{
    debug("");
    debug("Transaction completed");
    shouldBeTrue("dbCaptureFired");
    shouldBeTrue("captureFired");
    shouldBeTrue("requestFired");
    shouldBeFalse("bubbleFired");
    shouldBeFalse("dbBubbleFired");
    debug("");

    finishJSTest();
    return;
}

test();