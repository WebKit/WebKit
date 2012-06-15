// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_setVersion_abort.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's aborting setVersion");

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
    initialVersion = evalAndLog("initialVersion = db.version;");
    request = evalAndLog("request = db.setVersion('2')");
    request.onsuccess = cleanDatabase;
    request.onerror = unexpectedErrorCallback;
}

function cleanDatabase()
{
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    shouldBe("db.objectStoreNames.length", "1");

    index = evalAndLog("index = objectStore.createIndex('bar', 'baz');");
    shouldBe("objectStore.indexNames.length", "1");

    event.target.transaction.oncomplete = unexpectedSuccessCallback;
    event.target.transaction.onabort = postAbort;
    evalAndLog("event.target.transaction.abort();");
}

function postAbort()
{
    shouldBe("db.version", "initialVersion");
    shouldBe("db.objectStoreNames.length", "0");

    finishJSTest();
}

test();