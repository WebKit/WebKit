if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that IDBObjectStore.openKeyCursor() called with a key value returns a key-only cursor that does not expose 'value'.");

indexedDBTest(prepareDatabase, testKeyOnlyCursor);

function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = db.createObjectStore('store');");
    evalAndLog("objectStore.put('some value', 5);");
}

function testKeyOnlyCursor()
{
    debug("");
    debug("testKeyOnlyCursor():");
    trans = evalAndLog("trans = db.transaction('store', 'readonly');");
    trans.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = trans.objectStore('store');");

    request = evalAndLog("request = objectStore.openKeyCursor(5);");
    request.onerror = unexpectedErrorCallback;
    count = 0;
    request.onsuccess = function() {
        cursor = event.target.result;
        if (count == 0) {
            shouldBeNonNull("cursor");
            shouldBeFalse("cursor instanceof IDBCursorWithValue");
            shouldBeFalse("'value' in cursor");
            shouldBe("cursor.key", "5");
            shouldBe("cursor.primaryKey", "5");
            evalAndLog("cursor.continue();");
        } else if (count == 1) {
            shouldBeNull("cursor");
        } else {
            testFailed("Unexpected extra iteration");
        }
        count++;
    };
    trans.oncomplete = finishJSTest;
}
