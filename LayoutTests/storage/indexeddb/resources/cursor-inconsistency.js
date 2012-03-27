if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test consistency of IndexedDB's cursor objects.");

test();

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('cursor-inconsistency')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    var db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = setVersionSuccess;
    request.onerror = unexpectedErrorCallback;
}

function setVersionSuccess()
{
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = openBasicCursor;

    deleteAllObjectStores(db);

    var objectStore = evalAndLog("objectStore = db.createObjectStore('basicStore')");
    evalAndLog("objectStore.add('someValue1', 'someKey1').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('someValue2', 'someKey2').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('someValue3', 'someKey3').onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add('someValue4', 'someKey4').onerror = unexpectedErrorCallback");

}

function openBasicCursor()
{
    debug("openBasicCursor()");
    evalAndLog("trans = db.transaction(['basicStore'], IDBTransaction.READ_WRITE)");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = transactionComplete;

    keyRange = IDBKeyRange.lowerBound("someKey1");
    self.objectStore = evalAndLog("trans.objectStore('basicStore')");
    request = evalAndLog("objectStore.openCursor(keyRange)");
    request.onsuccess = checkCursor;
    request.onerror = unexpectedErrorCallback;
    counter = 1;
}

storedCursor = null;
function checkCursor()
{
    debug("")
    debug("checkCursor()");
    if (event.target.result == null) {
        shouldBe("counter", "5");
        return;
    }
    if (storedCursor == null)
      storedCursor = evalAndLog("storedCursor = event.target.result");

    shouldBeTrue("storedCursor === event.target.result");
    shouldBeEqualToString("storedCursor.key", "someKey" + counter);
    shouldBeEqualToString("event.target.result.key", "someKey" + counter);
    shouldBeEqualToString("storedCursor.value", "someValue" + counter);
    shouldBeEqualToString("event.target.result.value", "someValue" + counter);
    counter++;
    evalAndLog("event.target.result.continue()");
}

function transactionComplete()
{
    debug("transactionComplete()");
    finishJSTest();
}