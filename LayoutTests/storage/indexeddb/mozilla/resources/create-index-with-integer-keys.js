// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_create_index_with_integer_keys.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's creating index with integer keys");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    request = evalAndLog("indexedDB.open(name)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = setupObjectStoreAndCreateIndex;
    request.onerror = unexpectedErrorCallback;
}

function setupObjectStoreAndCreateIndex()
{
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'id' });");
    data = evalAndLog("data = { id: 16, num: 42 };");
    evalAndLog("objectStore.add(data);");
    index = evalAndLog("index = objectStore.createIndex('foo', 'num');");
    result = evalAndLog("result = index.openKeyCursor();");
    result.onsuccess = verifyKeyCursor;
    result.onerror = unexpectedErrorCallback;
}

function verifyKeyCursor()
{
    cursor = evalAndLog("cursor = event.target.result;");
    shouldBeFalse("cursor == null");
    shouldBe("cursor.key", "data.num");
    shouldBe("cursor.primaryKey", "data.id");
    finishJSTest();
}

test();