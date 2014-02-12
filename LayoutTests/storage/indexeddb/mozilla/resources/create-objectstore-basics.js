// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_create_objectStore.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's creating object store and updating properties");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    objectStoreInfo = [
        { name: "1", options: { autoIncrement: true } },
        { name: "2", options: { autoIncrement: false } },
        { name: "3", options: { keyPath: "" } },
        { name: "4", options: { keyPath: "", autoIncrement: true }, fail: true },
        { name: "5", options: { keyPath: "", autoIncrement: false } },
        { name: "6", options: { keyPath: "foo" } },
        { name: "7", options: { keyPath: "foo", autoIncrement: false } },
        { name: "8", options: { keyPath: "foo", autoIncrement: true } }
    ];

    for (var index in objectStoreInfo) {
        index = parseInt(index);
        info = objectStoreInfo[index];
        if (!info.fail) {
            objectStore = evalAndLog("objectStore = db.createObjectStore(info.name, info.options);");
            shouldBe("objectStore.name", "info.name");
            if (info.options && info.options.keyPath) {
                shouldBe("objectStore.keyPath", "info.options.keyPath");
            }
            shouldBe("objectStore.indexNames.length", "0");
            shouldBe("event.target.transaction.db", "db");
            shouldBeEqualToString("event.target.transaction.mode", "versionchange");
        } else {
            evalAndExpectException("objectStore = db.createObjectStore(info.name, info.options)", "DOMException.INVALID_ACCESS_ERR");
        }
    }

    finishJSTest();
}
