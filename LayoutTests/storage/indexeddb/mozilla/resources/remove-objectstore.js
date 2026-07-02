// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_remove_objectStore.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB deleting an object store");

indexedDBTest(prepareDatabase, checkObjectStore);
function prepareDatabase()
{
    db = event.target.result;
    shouldBe("db.objectStoreNames.length", "0");

    objectStoreName = evalAndLog("objectStoreName = 'Objects';");
    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName, { keyPath: 'foo' });");

    for (i = 0; i < 100; i++) {
        request = evalAndLog("request = objectStore.add({foo: i});");
        request.onerror = unexpectedErrorCallback;
    }
}

function checkObjectStore()
{
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.item(0)", "objectStoreName");
    evalAndLog("db.close()");

    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = inSetVersion2;
    request.onsuccess = setVersion3;
}

function inSetVersion2()
{
    db = event.target.result;
    evalAndLog("db.deleteObjectStore(objectStore.name);");
    shouldBe("db.objectStoreNames.length", "0");

    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName, { keyPath: 'foo' });");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.item(0)", "objectStoreName");

    request = evalAndLog("request = objectStore.openCursor();");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(event) {
        shouldBe("event.target.result", "null");
        deleteSecondObjectStore();
    };
}

function deleteSecondObjectStore()
{
    evalAndLog("db.deleteObjectStore(objectStore.name);");
    shouldBe("db.objectStoreNames.length", "0");
}

function setVersion3()
{
    evalAndLog("db.close()");

    evalAndLog("request = indexedDB.open(dbname, 3)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = inSetVersion3;
    request.onsuccess = finishJSTest;
}

function inSetVersion3()
{
    db = event.target.result;
    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName, { keyPath: 'foo' });");
    request = evalAndLog("request = objectStore.add({foo:'bar'});");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = deleteThirdObjectStore;
}

function deleteThirdObjectStore()
{
    evalAndLog("db.deleteObjectStore(objectStoreName);");
}
