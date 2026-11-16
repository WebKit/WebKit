// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_readonly_transactions.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's read/write transactions");

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
    request = evalAndLog("request = db.transaction([osName], 'readwrite').objectStore(osName).add({});");
    request.onsuccess = postAdd;
    request.onerror = unexpectedErrorCallback;
}

function postAdd()
{
    shouldBe("event.target.transaction.mode", "'readwrite'");
    key1 = evalAndLog("key1 = event.target.result;");
    request = evalAndLog("request = db.transaction(osName, 'readwrite').objectStore(osName).add({});");
    request.onsuccess = postAdd2;
    request.onerror = unexpectedErrorCallback;
}

function postAdd2()
{
    shouldBe("event.target.transaction.mode", "'readwrite'");
    key2 = evalAndLog("key2 = event.target.result;");
    request = evalAndLog("request = db.transaction([osName], 'readwrite').objectStore(osName).put({}, key1);");
    request.onsuccess = postPut;
    request.onerror = unexpectedErrorCallback;
}

function postPut()
{
    shouldBe("event.target.transaction.mode", "'readwrite'");
    request = evalAndLog("request = db.transaction(osName, 'readwrite').objectStore(osName).put({}, key2);");
    request.onsuccess = postPut2;
    request.onerror = unexpectedErrorCallback;
}

function postPut2()
{
    shouldBe("event.target.transaction.mode", "'readwrite'");
    request = evalAndLog("request = db.transaction([osName], 'readwrite').objectStore(osName).put({}, key1);");
    request.onsuccess = postPut3;
    request.onerror = unexpectedErrorCallback;
}

function postPut3()
{
    shouldBe("event.target.transaction.mode", "'readwrite'");
    request = evalAndLog("request = db.transaction(osName, 'readwrite').objectStore(osName).put({}, key1);");
    request.onsuccess = postPut4;
    request.onerror = unexpectedErrorCallback;
}

function postPut4()
{
    shouldBe("event.target.transaction.mode", "'readwrite'");
    request = evalAndLog("request = db.transaction([osName], 'readwrite').objectStore(osName).delete(key1);");
    request.onsuccess = postDelete;
    request.onerror = unexpectedErrorCallback;
}

function postDelete()
{
    shouldBe("event.target.transaction.mode", "'readwrite'");
    request = evalAndLog("request = db.transaction(osName, 'readwrite').objectStore(osName).delete(key2);");
    request.onsuccess = postDelete2;
    request.onerror = unexpectedErrorCallback;
}

function postDelete2()
{
    shouldBe("event.target.transaction.mode", "'readwrite'");
    finishJSTest();
}
