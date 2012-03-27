if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's openCursor.");

test();

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('cursor-index-delete')");
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
    trans.oncomplete = openCursor;

    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('test')");
    evalAndLog("objectStore.createIndex('testIndex', 'x')");

    evalAndLog("objectStore.add({x: 1}, 'myKey1')");
    evalAndLog("objectStore.add({x: 2}, 'myKey2')");
    evalAndLog("objectStore.add({x: 3}, 'myKey3')");
    evalAndLog("objectStore.add({x: 4}, 'myKey4')");
}

function openCursor()
{
    debug("openCursor1");
    evalAndLog("trans = db.transaction(['test'], IDBTransaction.READ_WRITE)");
    keyRange = IDBKeyRange.lowerBound(1);
    request = evalAndLog("trans.objectStore('test').index('testIndex').openCursor(keyRange)");
    request.onsuccess = cursorSuccess;
    request.onerror = unexpectedErrorCallback;
    counter = 1;
}

function cursorSuccess()
{
    if (event.target.result == null) {
        shouldBe("counter", "5");
        request = evalAndLog("trans.objectStore('test').index('testIndex').openCursor(keyRange)");
        request.onsuccess = cursorEmpty;
        request.onerror = unexpectedErrorCallback;
        return;
    }
    var deleteRequest = evalAndLog("event.target.result.delete()");
    deleteRequest.onerror = unexpectedErrorCallback;
    shouldBe("event.target.result.key", "counter++");
    evalAndLog("event.target.result.continue()");
}

function cursorEmpty()
{
    shouldBeNull("event.target.result");
    trans.oncomplete = addObject;
}

function addObject()
{
    evalAndLog("trans = db.transaction(['test'], IDBTransaction.READ_WRITE)");
    objectStore = evalAndLog("objectStore = trans.objectStore('test')");
    request = evalAndLog("objectStore.add({x: 1}, 'myKey1')");
    request.onsuccess = openCursor2;
    request.onerror = unexpectedErrorCallback;
}

function openCursor2()
{
    debug("openCursor2");
    evalAndLog("index = event.target.source.index('testIndex')");
    request = evalAndLog("index.openCursor(keyRange)");
    request.onsuccess = deleteObject;
    request.onerror = unexpectedErrorCallback;
}

function deleteObject()
{
    shouldBeNonNull(event.target.result);
    evalAndLog("event.target.result.delete()");
    request = evalAndLog("index.get(1)");
    request.onsuccess = verifyObjectDeleted;
    request.onerror = unexpectedErrorCallback;
}

function verifyObjectDeleted()
{
    shouldBe("event.target.result", "undefined");
    finishJSTest();
}