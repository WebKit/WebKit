if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB index population.");

indexedDBTest(prepareDatabase, doSetVersion2);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("transaction = event.target.transaction");
    transaction.onerror = unexpectedErrorCallback;
    transaction.onabort = unexpectedAbortCallback;
    store = evalAndLog("store = db.createObjectStore('store1')");
    evalAndLog("store.put({data: 'a', indexKey: 10}, 1)");
    evalAndLog("store.put({data: 'b', indexKey: 20}, 2)");
    evalAndLog("store.put({data: 'c', indexKey: 10}, 3)");
    evalAndLog("store.put({data: 'd', indexKey: 20}, 4)");
    evalAndLog("index = store.createIndex('index1', 'indexKey')");
    shouldBeTrue("index instanceof IDBIndex");
    shouldBeFalse("index.unique");
    request = evalAndLog("request = index.count(IDBKeyRange.bound(-Infinity, Infinity))");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        shouldBe("request.result", "4");
    };
}

function doSetVersion2() {
    debug("");
    debug("doSetVersion2():");
    evalAndLog("db.close()");
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onupgradeneeded = setVersion2;
    request.onblocked = unexpectedBlockedCallback;
}

function setVersion2()
{
    debug("");
    debug("setVersion2():");
    db = event.target.result;
    transaction2 = evalAndLog("transaction = request.transaction");
    transaction2.onabort = setVersion2Abort;
    transaction2.oncomplete = unexpectedCompleteCallback;

    var capturePhase = true;
    transaction2.addEventListener("error", unexpectedErrorCallback, !capturePhase);
    transaction2.addEventListener("error", unexpectedErrorCallback, capturePhase);
    db.addEventListener("error", unexpectedErrorCallback, !capturePhase);
    db.addEventListener("error", unexpectedErrorCallback, capturePhase);

    store = evalAndLog("store = db.createObjectStore('store2')");
    evalAndLog("store.put({data: 'a', indexKey: 10}, 1)");
    evalAndLog("store.put({data: 'b', indexKey: 20}, 2)");
    evalAndLog("store.put({data: 'c', indexKey: 10}, 3)");
    evalAndLog("store.put({data: 'd', indexKey: 20}, 4)");
    evalAndLog("index2 = store.createIndex('index2', 'indexKey', { unique: true })");
    shouldBeTrue("index2 instanceof IDBIndex");
    shouldBeTrue("index2.unique");
}

function setVersion2Abort()
{
    debug("");
    debug("setVersion2Abort():");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBeEqualToString("db.objectStoreNames[0]", "store1");
    finishJSTest();
}
