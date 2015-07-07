if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test the basics of IndexedDB's IDBCursor objects.");

indexedDBTest(prepareDatabase);
function prepareDatabase(evt)
{
    preamble(evt);
    db = event.target.result;

    evalAndLog("store = db.createObjectStore('storeName')");
    evalAndLog("index = store.createIndex('indexName', 'indexOn')");
    evalAndLog("store.put({indexOn: 'a'}, 0)");

    request = evalAndLog("store.openCursor()");
    request.onsuccess = onStoreOpenCursor;
    request.onerror = unexpectedErrorCallback;

    request = evalAndLog("store.openKeyCursor()");
    request.onsuccess = onStoreOpenKeyCursor;
    request.onerror = unexpectedErrorCallback;

    request = evalAndLog("index.openCursor()");
    request.onsuccess = onIndexOpenCursor;
    request.onerror = unexpectedErrorCallback;

    request = evalAndLog("index.openKeyCursor()");
    request.onsuccess = onIndexOpenKeyCursor;
    request.onerror = unexpectedErrorCallback;

    event.target.transaction.oncomplete = finishJSTest;
}

function checkCursorProperties() {
    shouldBeTrue("cursor instanceof IDBCursor");
    shouldBeTrue("'key' in cursor");
    shouldBeTrue("'primaryKey' in cursor");

    shouldBeTrue("'continue' in cursor");
    shouldBeEqualToString("typeof cursor.continue", "function");
    shouldBeTrue("'continuePrimaryKey' in cursor");
    shouldBeEqualToString("typeof cursor.continuePrimaryKey", "function");
    shouldBeTrue("'advance' in cursor");
    shouldBeEqualToString("typeof cursor.advance", "function");
    shouldBeTrue("'update' in cursor");
    shouldBeEqualToString("typeof cursor.update", "function");
    shouldBeTrue("'delete' in cursor");
    shouldBeEqualToString("typeof cursor.delete", "function");
}

function onStoreOpenCursor(evt) {
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    shouldBeNonNull("cursor");
    checkCursorProperties();

    shouldBe("cursor.key", "0");
    shouldBe("cursor.primaryKey", "0");

    shouldBeTrue("cursor instanceof IDBCursorWithValue");
    shouldBeTrue("'value' in cursor");
    shouldBeEqualToString("JSON.stringify(cursor.value)", '{"indexOn":"a"}');
}

function onStoreOpenKeyCursor(evt) {
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    shouldBeNonNull("cursor");
    checkCursorProperties();

    shouldBe("cursor.key", "0");
    shouldBeTrue("'primaryKey' in cursor");
    shouldBe("cursor.primaryKey", "0");

    shouldBeFalse("cursor instanceof IDBCursorWithValue");
    shouldBeFalse("'value' in cursor");
}

function onIndexOpenCursor(evt) {
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    shouldBeNonNull("cursor");
    checkCursorProperties();

    shouldBeEqualToString("cursor.key", "a");
    shouldBe("cursor.primaryKey", "0");

    shouldBeTrue("cursor instanceof IDBCursorWithValue");
    shouldBeTrue("'value' in cursor");
    shouldBeEqualToString("JSON.stringify(cursor.value)", '{"indexOn":"a"}');
}

function onIndexOpenKeyCursor(evt) {
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    shouldBeNonNull("cursor");
    checkCursorProperties();

    shouldBeEqualToString("cursor.key", "a");
    shouldBe("cursor.primaryKey", "0");

    shouldBeFalse("cursor instanceof IDBCursorWithValue");
    shouldBe("cursor.primaryKey", "0");
    shouldBeFalse("'value' in cursor");
}

