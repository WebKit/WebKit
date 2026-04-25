// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_remove_index.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's creating and deleting indexes");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    objectStoreName = "test store";
    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName, { keyPath: 'foo' });");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.item(0)", "objectStoreName");
    shouldBe("objectStore.indexNames.length", "0");

    indexName = "My Test Index";
    index = evalAndLog("index = objectStore.createIndex(indexName, 'foo');");
    shouldBe("objectStore.indexNames.length", "1");
    shouldBe("objectStore.indexNames.item(0)", "indexName");

    evalAndLog("objectStore.deleteIndex(indexName);");
    shouldBe("objectStore.indexNames.length", "0");

    finishJSTest();
}
