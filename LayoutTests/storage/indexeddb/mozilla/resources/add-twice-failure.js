// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_add_twice_failure.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB behavior adding the same property twice");

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

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    key = evalAndLog("key = 10");
    request = evalAndLog("request = objectStore.add({}, key);");
    request.onsuccess = addFirstSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addFirstSuccess()
{
    shouldBe("request.result", "key");
    request = evalAndLog("request = objectStore.add({}, key);");
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = addSecondExpectedError;
}

function addSecondExpectedError()
{
    shouldBe("event.target.errorCode", "IDBDatabaseException.CONSTRAINT_ERR");
    finishJSTest();
}

test();