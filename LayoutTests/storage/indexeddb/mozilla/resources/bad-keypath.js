// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_bad_keypath.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB adding property with invalid keypath");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'keyPath' });");
    request = evalAndLog("request = objectStore.add({ keyPath: 'foo' });");
    request.onsuccess = addFirstSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addFirstSuccess()
{
    evalAndExpectException("request = objectStore.add({});", "0", "'DataError'");
    finishJSTest();
}
