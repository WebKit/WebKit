// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_readonly_transactions.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's read/write transactions");

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

    osName = "test store";
    objectStore = evalAndLog("objectStore = db.createObjectStore(osName, { autoIncrement: true });");
    event.target.result.oncomplete = setVersionComplete;
}

function setVersionComplete()
{
    request = evalAndLog("request = db.transaction([osName], IDBTransaction.READ_WRITE).objectStore(osName).add({});");
    request.onsuccess = postAdd;
    request.onerror = unexpectedErrorCallback;
}

function postAdd()
{
    shouldBe("event.target.transaction.mode", "IDBTransaction.READ_WRITE");
    key1 = evalAndLog("key1 = event.target.result;");
    request = evalAndLog("request = db.transaction(osName, IDBTransaction.READ_WRITE).objectStore(osName).add({});");
    request.onsuccess = postAdd2;
    request.onerror = unexpectedErrorCallback;
}

function postAdd2()
{
    shouldBe("event.target.transaction.mode", "IDBTransaction.READ_WRITE");
    key2 = evalAndLog("key2 = event.target.result;");
    request = evalAndLog("request = db.transaction([osName], IDBTransaction.READ_WRITE).objectStore(osName).put({}, key1);");
    request.onsuccess = postPut;
    request.onerror = unexpectedErrorCallback;
}

function postPut()
{
    shouldBe("event.target.transaction.mode", "IDBTransaction.READ_WRITE");
    request = evalAndLog("request = db.transaction(osName, IDBTransaction.READ_WRITE).objectStore(osName).put({}, key2);");
    request.onsuccess = postPut2;
    request.onerror = unexpectedErrorCallback;
}

function postPut2()
{
    shouldBe("event.target.transaction.mode", "IDBTransaction.READ_WRITE");
    request = evalAndLog("request = db.transaction([osName], IDBTransaction.READ_WRITE).objectStore(osName).put({}, key1);");
    request.onsuccess = postPut3;
    request.onerror = unexpectedErrorCallback;
}

function postPut3()
{
    shouldBe("event.target.transaction.mode", "IDBTransaction.READ_WRITE");
    request = evalAndLog("request = db.transaction(osName, IDBTransaction.READ_WRITE).objectStore(osName).put({}, key1);");
    request.onsuccess = postPut4;
    request.onerror = unexpectedErrorCallback;
}

function postPut4()
{
    shouldBe("event.target.transaction.mode", "IDBTransaction.READ_WRITE");
    request = evalAndLog("request = db.transaction([osName], IDBTransaction.READ_WRITE).objectStore(osName).delete(key1);");
    request.onsuccess = postDelete;
    request.onerror = unexpectedErrorCallback;
}

function postDelete()
{
    shouldBe("event.target.transaction.mode", "IDBTransaction.READ_WRITE");
    request = evalAndLog("request = db.transaction(osName, IDBTransaction.READ_WRITE).objectStore(osName).delete(key2);");
    request.onsuccess = postDelete2;
    request.onerror = unexpectedErrorCallback;
}

function postDelete2()
{
    shouldBe("event.target.transaction.mode", "IDBTransaction.READ_WRITE");
    finishJSTest();
}

test();