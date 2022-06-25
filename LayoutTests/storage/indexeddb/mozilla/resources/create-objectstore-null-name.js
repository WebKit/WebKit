// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_create_objectStore.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's creating object store with null name");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = db.createObjectStore(null);");
    shouldBe("objectStore.name", "'null'");

    finishJSTest();
}
