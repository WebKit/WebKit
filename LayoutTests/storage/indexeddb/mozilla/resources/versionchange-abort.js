// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_setVersion_abort.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("When a versionchange transaction is aborted, the version and newly created object stores should be reset");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    request = event.target;
    request.onerror = null;

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    shouldBe("db.objectStoreNames.length", "1");

    index = evalAndLog("index = objectStore.createIndex('bar', 'baz');");
    shouldBe("objectStore.indexNames.length", "1");

    trans = request.transaction;
    trans.oncomplete = unexpectedCompleteCallback;
    trans.onabort = postAbort;
    evalAndLog("event.target.transaction.abort();");
}

function postAbort()
{
    shouldBe("db.version", "0");
    shouldBe("db.objectStoreNames.length", "0");

    finishJSTest();
}
