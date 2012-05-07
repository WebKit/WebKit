// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_create_objectStore.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's creating object store and updating properties");

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

    objectStoreInfo = [
        { name: "1", options: { autoIncrement: true } },
        { name: "2", options: { autoIncrement: false } },
        { name: "3", options: { keyPath: "" } },
        { name: "4", options: { keyPath: "", autoIncrement: true } },
        { name: "5", options: { keyPath: "", autoIncrement: false } },
        { name: "6", options: { keyPath: "foo" } },
        { name: "7", options: { keyPath: "foo", autoIncrement: false } },
        { name: "8", options: { keyPath: "foo", autoIncrement: true } }
    ];

    for (var index in objectStoreInfo) {
        index = parseInt(index);
        info = objectStoreInfo[index];
        objectStore = evalAndLog("objectStore = db.createObjectStore(info.name, info.options);");
        shouldBe("objectStore.name", "info.name");
        if (info.options && info.options.keyPath) {
            shouldBe("objectStore.keyPath", "info.options.keyPath");
        }
        shouldBe("objectStore.indexNames.length", "0");
        shouldBe("event.target.transaction.db", "db");
        shouldBe("event.target.transaction.readyState", "IDBTransaction.LOADING");
        shouldBe("event.target.transaction.mode", "'versionchange'");
    }

    finishJSTest();
}

test();