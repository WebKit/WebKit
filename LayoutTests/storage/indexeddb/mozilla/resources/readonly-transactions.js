// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_readonly_transactions.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's readonly transactions");

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
    request.onsuccess = cleanDatabase;
    request.onerror = unexpectedErrorCallback;
}

function cleanDatabase()
{
    deleteAllObjectStores(db);

    osName = "test store";
    objectStore = evalAndLog("objectStore = db.createObjectStore(osName, { autoIncrement: true });");
    event.target.result.oncomplete = setVersionComplete;
}

function setVersionComplete()
{
    evalAndExpectException("db.transaction([osName]).objectStore(osName).add({});", "IDBDatabaseException.READ_ONLY_ERR");
    evalAndExpectException("db.transaction(osName).objectStore(osName).add({});", "IDBDatabaseException.READ_ONLY_ERR");
    key1 = evalAndLog("key1 = 1;");
    key2 = evalAndLog("key2 = 2;");
    evalAndExpectException("db.transaction([osName]).objectStore(osName).put({}, key1);", "IDBDatabaseException.READ_ONLY_ERR");
    evalAndExpectException("db.transaction(osName).objectStore(osName).put({}, key2);", "IDBDatabaseException.READ_ONLY_ERR");
    evalAndExpectException("db.transaction([osName]).objectStore(osName).put({}, key1);", "IDBDatabaseException.READ_ONLY_ERR");
    evalAndExpectException("db.transaction(osName).objectStore(osName).put({}, key1);", "IDBDatabaseException.READ_ONLY_ERR");
    evalAndExpectException("db.transaction([osName]).objectStore(osName).delete(key1);", "IDBDatabaseException.READ_ONLY_ERR");
    evalAndExpectException("db.transaction(osName).objectStore(osName).delete(key2);", "IDBDatabaseException.READ_ONLY_ERR");
    finishJSTest();
}

test();