// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_key_requirements.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's behavior put()ing with null key");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    objectStore = evalAndLog("objectStore = db.createObjectStore('bar');");
    evalAndExpectException("objectStore.put({}, null);", "0", "'DataError'");
    finishJSTest();
}
