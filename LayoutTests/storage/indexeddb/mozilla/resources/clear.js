// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_clear.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's clearing an object store");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    description = "My Test Database";
    request = evalAndLog("indexedDB.open(name, description)");
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

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { autoIncrement: true });");
    request = evalAndLog("request = objectStore.add({});");
    request.onerror = unexpectedErrorCallback;
    event.target.result.oncomplete = clear;
}

function clear()
{
    evalAndExpectException("db.transaction('foo').objectStore('foo').clear();", "IDBDatabaseException.READ_ONLY_ERR");
    evalAndLog("db.transaction('foo', IDBTransaction.READ_WRITE).objectStore('foo').clear();");
    request = evalAndLog("request = db.transaction('foo').objectStore('foo').openCursor();");
    request.onsuccess = areWeClearYet;
    request.onerror = unexpectedErrorCallback;
}

function areWeClearYet()
{
    cursor = evalAndLog("cursor = request.result;");
    shouldBe("cursor", "null");
    finishJSTest();
}

test();