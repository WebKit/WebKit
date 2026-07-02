// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_readonly_transactions.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's readonly transactions");

indexedDBTest(prepareDatabase, setVersionComplete);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    osName = "test store";
    objectStore = evalAndLog("objectStore = db.createObjectStore(osName, { autoIncrement: true });");
}

function setVersionComplete()
{
    evalAndExpectException("db.transaction([osName]).objectStore(osName).add({});", "0", "'ReadOnlyError'");
    evalAndExpectException("db.transaction(osName).objectStore(osName).add({});", "0", "'ReadOnlyError'");
    key1 = evalAndLog("key1 = 1;");
    key2 = evalAndLog("key2 = 2;");
    evalAndExpectException("db.transaction([osName]).objectStore(osName).put({}, key1);", "0", "'ReadOnlyError'");
    evalAndExpectException("db.transaction(osName).objectStore(osName).put({}, key2);", "0", "'ReadOnlyError'");
    evalAndExpectException("db.transaction([osName]).objectStore(osName).put({}, key1);", "0", "'ReadOnlyError'");
    evalAndExpectException("db.transaction(osName).objectStore(osName).put({}, key1);", "0", "'ReadOnlyError'");
    evalAndExpectException("db.transaction([osName]).objectStore(osName).delete(key1);", "0", "'ReadOnlyError'");
    evalAndExpectException("db.transaction(osName).objectStore(osName).delete(key2);", "0", "'ReadOnlyError'");
    finishJSTest();
}
