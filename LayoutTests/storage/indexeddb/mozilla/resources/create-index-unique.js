// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_create_index.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's creating unique index and updating indexNames");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    description = "My Test Database";
    request = evalAndLog("indexedDB.open(name, description)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = createAndVerifyIndex;
    request.onerror = unexpectedErrorCallback;
}

function createAndVerifyIndex()
{
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('a', { keyPath: 'id', autoIncrement: true });");

    indexName = "1";
    indexKeyPath = "unique_value";
    index = evalAndLog("index = objectStore.createIndex(indexName, indexKeyPath, { unique: true });", "IDBDatabaseException.CONSTRAINT_ERR");
    shouldBe("index.name", "indexName");
    shouldBe("index.keyPath", "indexKeyPath");
    shouldBe("index.unique", "true");
    shouldBe("objectStore.indexNames.length", "1");

    foundNewlyCreatedIndex = false;
    for (var k = 0; k < objectStore.indexNames.length; k++) {
        if (objectStore.indexNames.item(k) == indexName) {
            foundNewlyCreatedIndex = true;
        }
    }
    shouldBeTrue("foundNewlyCreatedIndex");
    shouldBe("event.target.transaction.db", "db");
    shouldBe("event.target.transaction.readyState", "IDBTransaction.LOADING");
    shouldBe("event.target.transaction.mode", "IDBTransaction.VERSION_CHANGE");
    finishJSTest();
}

test();