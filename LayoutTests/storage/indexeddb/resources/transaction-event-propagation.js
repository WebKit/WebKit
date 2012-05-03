if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test event propogation on IDBTransaction.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('transaction-event-propagation')");
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
    debug("Verifing abort");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.onabort = abortFiredCallback");
    evalAndLog("trans.oncomplete = unexpectedAbortCallback");
    evalAndLog("db.addEventListener('abort', dbAbortCaptureCallback, true)");
    evalAndLog("db.addEventListener('abort', dbAbortBubbleCallback, false)");
    evalAndLog("db.addEventListener('complete', unexpectedCompleteCallback, true)");
    evalAndLog("db.addEventListener('complete', unexpectedCompleteCallback, false)");
    store = evalAndLog("store = trans.objectStore('storeName')");
    evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key')");
    dbCaptureFired = false;
    abortFired = false;
    dbBubbleFired1 = false;
}

function dbAbortCaptureCallback()
{
    debug("");
    debug("In IDBDatabase abort capture");
    shouldBeFalse("dbCaptureFired");
    shouldBeFalse("abortFired");
    shouldBeFalse("dbBubbleFired1");
    shouldBe("event.target", "trans");
    shouldBe("event.currentTarget", "db");
    dbCaptureFired = true;
}

function abortFiredCallback()
{
    debug("");
    debug("In abort handler");
    shouldBeTrue("dbCaptureFired");
    shouldBeFalse("abortFired");
    shouldBeFalse("dbBubbleFired1");
    shouldBe("event.target", "trans");
    shouldBe("event.currentTarget", "trans");
    abortFired = true;
}

function dbAbortBubbleCallback()
{
    debug("");
    debug("In IDBDatabase error bubble");
    shouldBeTrue("dbCaptureFired");
    shouldBeTrue("abortFired");
    shouldBeFalse("dbBubbleFired1");
    shouldBe("event.target", "trans");
    shouldBe("event.currentTarget", "db");
    dbBubbleFired1 = true;
    debug("");
    debug("Verifing success.");
    trans = evalAndLog("trans = db.transaction(['storeName'], 'readwrite')");
    evalAndLog("trans.oncomplete = completeFiredCallback");
    evalAndLog("trans.onabort = unexpectedAbortCallback");
    evalAndLog("db.removeEventListener('abort', dbAbortCaptureCallback, true)");
    evalAndLog("db.removeEventListener('abort', dbAbortBubbleCallback, false)");
    evalAndLog("db.removeEventListener('complete', unexpectedCompleteCallback, true)");
    evalAndLog("db.removeEventListener('complete', unexpectedCompleteCallback, false)");
    evalAndLog("db.addEventListener('complete', dbCompleteCaptureCallback, true)");
    evalAndLog("db.addEventListener('complete', dbCompleteBubbleCallback, false)");
    evalAndLog("db.addEventListener('abort', unexpectedAbortCallback, true)");
    evalAndLog("db.addEventListener('abort', unexpectedAbortCallback, false)");
    store = evalAndLog("store = trans.objectStore('storeName')");
    evalAndLog("store.add({x: 'value', y: 'zzz'}, 'key2')");
    dbCaptureFired = false;
    completeFired = false;
    dbBubbleFired2 = false;
}

function dbCompleteCaptureCallback()
{
    debug("");
    debug("In IDBDatabase complete capture");
    shouldBeFalse("dbCaptureFired");
    shouldBeFalse("completeFired");
    shouldBeFalse("dbBubbleFired2");
    shouldBe("event.target", "trans");
    shouldBe("event.currentTarget", "db");
    dbCaptureFired = true;
}

function completeFiredCallback()
{
    debug("");
    debug("In IDBRequest handler");
    shouldBeTrue("dbCaptureFired");
    shouldBeFalse("completeFired");
    shouldBeFalse("dbBubbleFired2");
    shouldBe("event.target", "trans");
    shouldBe("event.currentTarget", "trans");
    completeFired = true;
    debug("");
    finishJSTest();
}

function dbCompleteBubbleCallback()
{
    debug("");
    debug("In IDBDatabase complete bubble");
    testFailed("Complete bubble callback should NOT fire");
    dbBubbleFired2 = true;
}

test();