// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_event_source.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's event.target.source in success callbacks");

indexedDBTest(prepareDatabase, finishJSTest);
function prepareDatabase()
{
    db = event.target.result;
    source = evalAndLog("source = event.target.source;");
    shouldBeNull("source");

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
}
