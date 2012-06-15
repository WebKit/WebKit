// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_bad_keypath.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB adding property with invalid keypath");

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

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'keyPath' });");
    request = evalAndLog("request = objectStore.add({ keyPath: 'foo' });");
    request.onsuccess = addFirstSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addFirstSuccess()
{
    evalAndExpectException("request = objectStore.add({});", "IDBDatabaseException.DATA_ERR");
    finishJSTest();
}

test();