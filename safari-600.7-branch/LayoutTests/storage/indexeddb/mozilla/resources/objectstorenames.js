// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_global_data.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's objectStoreNames array");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    objectStoreName = "a";
    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName, { keyPath: 'id', autoIncrement: true });");

    shouldBeTrue("'objectStoreNames' in db");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.item(0)", "objectStoreName");
    finishJSTest();
}
