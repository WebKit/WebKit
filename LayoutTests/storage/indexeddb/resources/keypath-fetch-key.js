if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB keyPaths");

testData = [{ name: "simple identifier",
              value: {id:10},
              keyPath: "id",
              key: 10 },
            { name: "simple identifiers",
              value: {id1:10, id2:20},
              keyPath: "id1",
              key: 10 },
            { name: "nested identifiers",
              value: {outer:{inner:10}},
              keyPath: "outer.inner",
              key: 10 },
            { name: "nested identifiers with distractions",
              value: {outer:{inner:10}, inner:{outer:20}},
              keyPath: "outer.inner",
              key: 10 },
];
nextToOpen = 0;

indexedDBTest(prepareDatabase);
var db = null;
var trans = null;
function prepareDatabase()
{
    db = db || event.target.result;
    if (!trans) {
        trans = event.target.transaction;
        trans.onabort = unexpectedAbortCallback;
    }
    debug("");
    debug("testing " + testData[nextToOpen].name);
    deleteAllObjectStores(db);
    objectStore = evalAndLog("objectStore = db.createObjectStore(testData[nextToOpen].name, {keyPath: testData[nextToOpen].keyPath});");
    result = evalAndLog("result = objectStore.add(testData[nextToOpen].value);");
    result.onerror = unexpectedErrorCallback;
    result.onsuccess = openCursor;
}

function openCursor()
{
    result = evalAndLog("result = objectStore.openCursor();");
    result.onerror = unexpectedErrorCallback;
    result.onsuccess = checkCursor;
}

function checkCursor()
{
    cursor = evalAndLog("cursor = event.target.result;");
    if (cursor) {
        shouldBe("cursor.key", "testData[nextToOpen].key");
    } else {
        testFailed("cursor is null");
    }
    if (++nextToOpen < testData.length) {
        prepareDatabase();
    } else {
        finishJSTest();
    }
}
