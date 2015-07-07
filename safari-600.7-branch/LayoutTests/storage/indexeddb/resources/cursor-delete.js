if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's openCursor.");

indexedDBTest(prepareDatabase, openCursor);
function prepareDatabase()
{
    db = event.target.result;
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;

    var objectStore = evalAndLog("objectStore = db.createObjectStore('test')");
    evalAndLog("objectStore.add('myValue1', 'myKey1')");
    evalAndLog("objectStore.add('myValue2', 'myKey2')");
    evalAndLog("objectStore.add('myValue3', 'myKey3')");
    evalAndLog("objectStore.add('myValue4', 'myKey4')");
}

function openCursor()
{
    debug("openCursor1");
    evalAndLog("trans = db.transaction(['test'], 'readwrite')");
    keyRange = IDBKeyRange.lowerBound("myKey1");
    request = evalAndLog("trans.objectStore('test').openCursor(keyRange)");
    request.onsuccess = cursorSuccess;
    request.onerror = unexpectedErrorCallback;
    counter = 1;
}

function cursorSuccess()
{
    if (event.target.result == null) {
        shouldBe("counter", "5");
        request = evalAndLog("trans.objectStore('test').openCursor(keyRange)");
        request.onsuccess = cursorEmpty;
        request.onerror = unexpectedErrorCallback;
        return;
    }
    evalAndLog("event.target.result.delete()");
    shouldBeEqualToString("event.target.result.value", "myValue" + counter++);
    evalAndLog("event.target.result.continue()");
}

function cursorEmpty()
{
    shouldBeNull("event.target.result");
    trans.oncomplete = addObject;
}

function addObject()
{
    evalAndLog("trans = db.transaction(['test'], 'readwrite')");
    objectStore = evalAndLog("objectStore = trans.objectStore('test')");
    request = evalAndLog("objectStore.add('myValue1', 'myKey1')");
    request.onsuccess = openCursor2;
    request.onerror = unexpectedErrorCallback;
}

function openCursor2()
{
    debug("openCursor2");
    request = evalAndLog("objectStore.openCursor(keyRange)");
    request.onsuccess = deleteObject;
    request.onerror = unexpectedErrorCallback;
}

function deleteObject()
{
    shouldBeNonNull(event.target.result);
    evalAndLog("event.target.result.delete()");
    request = evalAndLog("objectStore.get('myKey1')");
    request.onsuccess = verifyObjectDeleted;
    request.onerror = unexpectedErrorCallback;
}

function verifyObjectDeleted()
{
    shouldBe("event.target.result", "undefined");
    finishJSTest();
}
