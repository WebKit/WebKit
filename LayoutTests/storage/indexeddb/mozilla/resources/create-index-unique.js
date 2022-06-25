// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_create_index.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's creating unique index and updating indexNames");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = db.createObjectStore('a', { keyPath: 'id', autoIncrement: true });");

    indexName = "1";
    indexKeyPath = "unique_value";
    index = evalAndLog("index = objectStore.createIndex(indexName, indexKeyPath, { unique: true });");
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
    shouldBe("event.target.transaction.mode", "'versionchange'");
    finishJSTest();
}
