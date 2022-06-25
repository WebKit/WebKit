// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_odd_result_order.html?raw=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB: result property after deleting existing and non-existing record");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = db.createObjectStore('foo', { keyPath: 'id', autoIncrement: true });");
    request = evalAndLog("request = objectStore.add({});");
    request.onsuccess = deleteRecord1;
    request.onerror = unexpectedErrorCallback;
}

function deleteRecord1()
{
    id = evalAndLog("id = event.target.result;");
    request = evalAndLog("request = objectStore.delete(id);");
    request.onsuccess = deleteRecord2;
    request.onerror = unexpectedErrorCallback;
}

function deleteRecord2()
{
    shouldBe("event.target.result", "undefined");
    request = evalAndLog("request = objectStore.delete(id);");
    request.onsuccess = finalCheck;
    request.onerror = unexpectedSuccessCallback;
}

function finalCheck()
{
    shouldBe("event.target.result", "undefined");
    finishJSTest();
}
