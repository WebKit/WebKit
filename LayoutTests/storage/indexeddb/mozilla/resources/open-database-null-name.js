// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_open_empty_db.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB: should NOT throw when opening a database with a null name");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open(null);");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");
    shouldBe("db.name", "'null'");
    finishJSTest();
}

test();