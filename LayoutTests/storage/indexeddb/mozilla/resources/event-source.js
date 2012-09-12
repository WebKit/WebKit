// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_event_source.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's event.target.source in success callbacks");

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
    source = evalAndLog("source = event.target.source;");
    shouldBeNull("source");

    db = evalAndLog("db = event.target.result");
    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = cleanDatabase;
    request.onerror = unexpectedErrorCallback;
}

function cleanDatabase()
{
    source = evalAndLog("source = event.target.source;");
    shouldBe("source", "db");

    deleteAllObjectStores(db);

    objectStoreName = "Objects";
    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName, { autoIncrement: true });");
    request = evalAndLog("request = objectStore.add({});");
    request.onsuccess = areWeDoneYet;
    request.onerror = unexpectedErrorCallback;
}

function areWeDoneYet()
{
    source = evalAndLog("source = event.target.source;");
    shouldBe("source", "objectStore");
    finishJSTest();
}

test();