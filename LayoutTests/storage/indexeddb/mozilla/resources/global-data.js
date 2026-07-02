// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_global_data.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's opening DB more than once");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db1 = event.target.result;

    objectStoreName = "Objects";
    objectStoreOptions = { keyPath: 'id', autoIncrement: true };
    evalAndLog("db1.createObjectStore(objectStoreName, objectStoreOptions);");

    request = evalAndLog("indexedDB.open(dbname);");
    request.onsuccess = open2Success;
    request.onerror = unexpectedErrorCallback;
}

function open2Success()
{
    db2 = evalAndLog("db2 = event.target.result");

    shouldBeTrue("db1 !== db2");
    shouldBe("db1.objectStoreNames.length", "1");
    shouldBe("db1.objectStoreNames.item(0)", "objectStoreName");
    shouldBe("db2.objectStoreNames.length", "1");
    shouldBe("db2.objectStoreNames.item(0)", "objectStoreName");

    objectStore1 = evalAndLog("objectStore1 = db1.transaction(objectStoreName).objectStore(objectStoreName);");
    shouldBe("objectStore1.name", "objectStoreName");
    shouldBe("objectStore1.keyPath", "objectStoreOptions.keyPath");

    objectStore2 = evalAndLog("objectStore2 = db2.transaction(objectStoreName).objectStore(objectStoreName);");
    shouldBeTrue("objectStore1 !== objectStore2");
    shouldBe("objectStore2.name", "objectStoreName");
    shouldBe("objectStore2.keyPath", "objectStoreOptions.keyPath");

    finishJSTest();
}
