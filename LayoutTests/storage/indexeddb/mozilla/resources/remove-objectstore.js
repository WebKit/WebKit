// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_remove_objectStore.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB deleting an object store");

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
    shouldBe("db.objectStoreNames.length", "0");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = createAndPopulateObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function createAndPopulateObjectStore()
{
    var v1_transaction = event.target.result;
    deleteAllObjectStores(db);

    objectStoreName = evalAndLog("objectStoreName = 'Objects';");
    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName, { keyPath: 'foo' });");

    for (i = 0; i < 100; i++) {
        request = evalAndLog("request = objectStore.add({foo: i});");
        request.onerror = unexpectedErrorCallback;
    }
    v1_transaction.oncomplete = checkObjectStore;
}

function checkObjectStore()
{
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.item(0)", "objectStoreName");

    request = db.setVersion('2');
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postSetVersion2;
}

function postSetVersion2()
{
    var v2_transaction = event.target.result;
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
    v2_transaction.oncomplete = setVersion3;
}

function deleteSecondObjectStore()
{
    evalAndLog("db.deleteObjectStore(objectStore.name);");
    shouldBe("db.objectStoreNames.length", "0");
}

function setVersion3()
{
    request = evalAndLog("request = db.setVersion('3');");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postSetVersion3;
}

function postSetVersion3()
{
    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName, { keyPath: 'foo' });");
    request = evalAndLog("request = objectStore.add({foo:'bar'});");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = deleteThirdObjectStore;
}

function deleteThirdObjectStore()
{
    evalAndLog("db.deleteObjectStore(objectStoreName);");
    finishJSTest();
}

test();