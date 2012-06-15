// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_key_requirements.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's behavior deleting entry with no key");

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

    request = evalAndLog("request = db.setVersion('version 1')");
    request.onsuccess = cleanDatabaseAndCreateObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function cleanDatabaseAndCreateObjectStore()
{
    deleteAllObjectStores(db);
    objectStore = evalAndLog("objectStore = db.createObjectStore('bar');");
    evalAndExpectException("objectStore.delete(null);", "IDBDatabaseException.DATA_ERR");
    finishJSTest();
}

test();