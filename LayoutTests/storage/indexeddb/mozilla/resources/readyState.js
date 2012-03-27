// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_request_readyState.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB readyState property during different operations");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    description = "My Test Database";
    request = evalAndLog("indexedDB.open(name, description)");
    shouldBe("request.readyState", "IDBRequest.LOADING");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    shouldBe("request.readyState", "IDBRequest.DONE");
    db = evalAndLog("db = event.target.result");
    request = evalAndLog("request = db.setVersion('1')");
    shouldBe("request.readyState", "IDBRequest.LOADING");
    request.onsuccess = setupObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function setupObjectStore()
{
    debug("setupObjectStore():");
    shouldBe("request.readyState", "IDBRequest.DONE");
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    key = evalAndLog("key = 10;");
    request = evalAndLog("request = objectStore.add({}, key);");
    shouldBe("request.readyState", "IDBRequest.LOADING");
    request.onsuccess = getRecord;
    request.onerror = unexpectedErrorCallback;
}

function getRecord()
{
    debug("getRecord():");
    shouldBe("request.readyState", "IDBRequest.DONE");
    shouldBe("event.target.result", "key");
    request = evalAndLog("request = objectStore.get(key);");
    shouldBe("request.readyState", "IDBRequest.LOADING");
    request.onsuccess = finalCheck;
    request.onerror = unexpectedErrorCallback;
}

function finalCheck()
{
    debug("finalCheck():");
    shouldBe("request.readyState", "IDBRequest.DONE");
    shouldBeFalse("event.target.result == null");
    finishJSTest();
}

test();