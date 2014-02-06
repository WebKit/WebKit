// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_clear.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's clearing an object store");

indexedDBTest(prepareDatabase, clear);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { autoIncrement: true });");
    request = evalAndLog("request = objectStore.add({});");
    request.onerror = unexpectedErrorCallback;
}

function clear()
{
    evalAndExpectException("db.transaction('foo').objectStore('foo').clear();", "0", "'ReadOnlyError'");
    transaction = evalAndLog("db.transaction('foo', 'readwrite')");
    evalAndLog("transaction.objectStore('foo').clear();");
    transaction.oncomplete = cleared;
    transaction.onabort = unexpectedAbortCallback;
}

function cleared()
{
    request = evalAndLog("request = db.transaction('foo').objectStore('foo').openCursor();");
    request.onsuccess = areWeClearYet;
    request.onerror = unexpectedErrorCallback;
}

// The version of this test that existed up until revision ~163500 had the following areWeClearYet handler.
// The intention was apparently to verify that calling openCursor() on an empty object store returned a null cursor.
/*
function areWeClearYet()
{
    cursor = evalAndLog("cursor = request.result;");
    shouldBe("cursor", "null");
    finishJSTest();
}
*/

// The spec's current description of IDBObjectStore.openCursor(), as found here:
// http://www.w3.org/TR/IndexedDB/#widl-IDBObjectStore-openCursor-IDBRequest-any-range-IDBCursorDirection-direction
// does not allow for a null cursor to be returned. It states:
// "...this method creates a cursor. The cursor must implement the IDBCursorWithValue interface."
// and then gives no allowance for not returning that created cursor.
// ---
// So our current reading of the spec is that a cursor must be returned, but it must be pointing to an undefined key/value record.
function areWeClearYet()
{
    cursor = evalAndLog("cursor = request.result;");
    shouldBe("cursor.key", "undefined");
    shouldBe("cursor.value", "undefined");
    finishJSTest();
}
