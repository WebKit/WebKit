if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test openCursor/openKeyCursor with raw IDBKeys.");

var objectStoreData = [
    { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
    { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
    { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
    { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
    { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
    { key: "237-23-7737", value: { name: "Pat", height: 65, weight: 100 } },
    { key: "237-23-7738", value: { name: "Leo", height: 65, weight: 180 } },
    { key: "237-23-7739", value: { name: "Jef", height: 65, weight: 120 } },
    { key: "237-23-7740", value: { name: "Sam", height: 71, weight: 110 } },
    { key: "237-23-7741", value: { name: "Bug", height: 63, weight: 100 } },
    { key: "237-23-7742", value: { name: "Tub", height: 69, weight: 180 } },
    { key: "237-23-7743", value: { name: "Rug", height: 77, weight: 120 } },
    { key: "237-23-7744", value: { name: "Pug", height: 66, weight: 110 } },
];

var indexData = [
    { name: "name", keyPath: "name", options: { unique: true } },
    { name: "height", keyPath: "height", options: { } },
    { name: "weight", keyPath: "weight", options: { unique: false } }
];

function test()
{
    removeVendorPrefixes();
    name = window.location.pathname;
    request = evalAndLog("indexedDB.open(name)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    db = evalAndLog("db = event.target.result");

    objectStoreName = "People";

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = createAndPopulateObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function createAndPopulateObjectStore()
{
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore(objectStoreName);");
    trans = event.target.result;
    trans.onabort = unexpectedAbortCallback;

    debug("First, add all our data to the object store.");
    addedData = 0;
    for (i in objectStoreData) {
        request = evalAndLog("request = objectStore.add(objectStoreData[i].value, objectStoreData[i].key);");
        request.onerror = unexpectedErrorCallback;
    }
    createIndexes();
    trans.oncomplete = testAll;
}

function createIndexes()
{
    debug("Now create the indexes.");
    for (i in indexData) {
        evalAndLog("objectStore.createIndex(indexData[i].name, indexData[i].keyPath, indexData[i].options);");
    }
}

function testAll()
{
    testObjectStore();
}

function testObjectStore()
{
    debug("testObjectStore()");
    evalAndLog("trans = db.transaction(objectStoreName, 'readwrite')");
    trans.onabort = unexpectedAbortCallback;
    objectStore = evalAndLog("objectStore = trans.objectStore(objectStoreName)");

    evalAndLog("request = objectStore.openCursor('237-23-7739')");
    var count = 0;
    request.onsuccess = function() {
        cursor = event.target.result;

        if (count == 0) {
            shouldBe("cursor.key", "'237-23-7739'");
            shouldBe("JSON.stringify(cursor.value)", "JSON.stringify(objectStoreData[7].value)");
            shouldBe("cursor.primaryKey", "cursor.key");
            evalAndLog("cursor.continue()");
        } else if (count == 1) {
            shouldBeNull("cursor");
        } else {
            testFailed("Too much iteration");
        }
        count++;
    };
    trans.oncomplete = testIndex;
    request.onerror = unexpectedErrorCallback;
}

function testIndex()
{
    evalAndLog("trans = db.transaction(objectStoreName, 'readwrite')");
    objectStore = evalAndLog("objectStore = trans.objectStore(objectStoreName)");
    index = evalAndLog("index = objectStore.index('weight')");

    evalAndLog("request = index.openCursor(180)");
    var count = 0;
    request.onsuccess = function() {
        cursor = event.target.result;

        if (count == 0) {
            shouldBe("cursor.key", '180');
            shouldBe("JSON.stringify(cursor.value)",
                     "JSON.stringify(objectStoreData[2].value)");
            shouldBe("cursor.primaryKey", "'237-23-7734'");
            evalAndLog("cursor.continue()");
        } else if (count == 1) {
            shouldBe("cursor.key", '180');
            shouldBe("JSON.stringify(cursor.value)",
                     "JSON.stringify(objectStoreData[6].value)");
            shouldBe("cursor.primaryKey", "'237-23-7738'");
            evalAndLog("cursor.continue()");
        } else if (count == 2) {
            shouldBe("cursor.key", '180');
            shouldBe("JSON.stringify(cursor.value)",
                     "JSON.stringify(objectStoreData[10].value)");
            shouldBe("cursor.primaryKey", "'237-23-7742'");
            evalAndLog("cursor.continue()");
        } else if (count == 3) {
            shouldBeNull("cursor");
        } else {
            testFailed("Too much iteration");
        }
        count++;
    };
    trans.oncomplete = testIndexWithKey;
    request.onerror = unexpectedErrorCallback;
}

function testIndexWithKey()
{
    evalAndLog("trans = db.transaction(objectStoreName, 'readwrite')");
    objectStore = evalAndLog("objectStore = trans.objectStore(objectStoreName)");
    index = evalAndLog("index = objectStore.index('weight')");

    evalAndLog("request = index.openKeyCursor(180)");
    var count = 0;
    request.onsuccess = function() {
        cursor = event.target.result;

        if (count == 0) {
            shouldBe("cursor.key", '180');
            shouldBe("cursor.primaryKey", "'237-23-7734'");
            evalAndLog("cursor.continue()");
        } else if (count == 1) {
            shouldBe("cursor.key", '180');
            shouldBe("cursor.primaryKey", "'237-23-7738'");
            evalAndLog("cursor.continue()");
        } else if (count == 2) {
            shouldBe("cursor.key", '180');
            shouldBe("cursor.primaryKey", "'237-23-7742'");
            evalAndLog("cursor.continue()");
        } else if (count == 3) {
            shouldBeNull("cursor");
        } else {
            testFailed("Too much iteration");
        }
        count++;
    };
    request.onerror = unexpectedErrorCallback;
    trans.oncomplete = finishJSTest;
}


test();
