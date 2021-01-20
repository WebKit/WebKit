// Adapted from a Mozilla test placed in the public domain.
// Taken from git repository https://github.com/mozilla/gecko-dev.git and the path ./dom/indexedDB/test/unit/test_index_getAll.js

description("Test IDBIndex.getAll()");

function continueWithEvent(event)
{
    testGenerator.next(event);
}

function log(msg)
{
	document.body.innerHTML += msg + "<br>";
}

function is(a, b, pass)
{
    if (areObjectsEqual(a, b))
        log("PASS: " + pass);
    else
        log("FAIL: " + a + " should be " + b + " but is not.");
}

function* testSteps()
{
    const name = window.location.pathname;
    const objectStoreName = "People";

    let objectStoreData = [
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7737", value: { name: "Pat", height: 65 } }
    ];

    let indexData = [
        { name: "name", keyPath: "name", options: { unique: true } },
        { name: "height", keyPath: "height", options: { unique: false } },
        { name: "weight", keyPath: "weight", options: { unique: false } }
    ];

    let objectStoreDataNameSort = [
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7737", value: { name: "Pat", height: 65 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } }
    ];

    let objectStoreDataWeightSort = [
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
    ];

    let objectStoreDataHeightSort = [
        { key: "237-23-7733", value: { name: "Ann", height: 52, weight: 110 } },
        { key: "237-23-7735", value: { name: "Sue", height: 58, weight: 130 } },
        { key: "237-23-7732", value: { name: "Bob", height: 60, weight: 120 } },
        { key: "237-23-7736", value: { name: "Joe", height: 65, weight: 150 } },
        { key: "237-23-7737", value: { name: "Pat", height: 65 } },
        { key: "237-23-7734", value: { name: "Ron", height: 73, weight: 180 } }
    ];
	
    let request = indexedDB.deleteDatabase(name);
	request.onerror = finishJSTest;
    request.onsuccess = continueWithEvent;
    event = yield;

    request = indexedDB.open(name, 1);
    request.onerror = finishJSTest;
    request.onupgradeneeded = continueWithEvent;
    event = yield;

    let db = event.target.result;
	event.target.transaction.oncomplete = continueWithEvent;
    let objectStore = db.createObjectStore(objectStoreName);

    // First, add all our data to the object store.
    let addedData = 0;
    for (let i in objectStoreData) {
        request = objectStore.add(objectStoreData[i].value, objectStoreData[i].key);
        request.onerror = finishJSTest;
        request.onsuccess = function(event) {
            if (++addedData == objectStoreData.length) {
                testGenerator.next(event);
            }
        }
    }
    yield;

    // Now create the indexes.
    for (let i in indexData) {
        objectStore.createIndex(indexData[i].name, indexData[i].keyPath, indexData[i].options);
    }

    is(objectStore.indexNames.length, indexData.length, "Good index count");
    yield; // Completion of version change transaction

    objectStore = db.transaction(objectStoreName).objectStore(objectStoreName);
    request = objectStore.index("height").getAll(65);
    request.onerror = finishJSTest;
    request.onsuccess = continueWithEvent;
	
    event = yield;

    is(event.target.result instanceof Array, true, "Got an array object");
    is(event.target.result.length, 2, "Correct length");

    for (let i in event.target.result) {
        is(event.target.result[i], objectStoreDataHeightSort[parseInt(i) + 3].value, "Correct value");
    }

    request = objectStore.index("height").getAll(65, 0);
    request.onerror = finishJSTest;
    request.onsuccess = continueWithEvent;
    event = yield;

    is(event.target.result instanceof Array, true, "Got an array object");
    is(event.target.result.length, 2, "Correct length");

    for (let i in event.target.result) {
        is(event.target.result[i], objectStoreDataHeightSort[parseInt(i) + 3].value, "Correct value");
    }

    request = objectStore.index("height").getAll(65, null);
    request.onerror = finishJSTest;
    request.onsuccess = continueWithEvent;
    event = yield;

    is(event.target.result instanceof Array, true, "Got an array object");
    is(event.target.result.length, 2, "Correct length");

    for (let i in event.target.result) {
        is(event.target.result[i], objectStoreDataHeightSort[parseInt(i) + 3].value, "Correct value");
    }

    request = objectStore.index("height").getAll(65, undefined);
    request.onerror = finishJSTest;
    request.onsuccess = continueWithEvent;
    event = yield;

    is(event.target.result instanceof Array, true, "Got an array object");
    is(event.target.result.length, 2, "Correct length");

    for (let i in event.target.result) {
        is(event.target.result[i], objectStoreDataHeightSort[parseInt(i) + 3].value, "Correct value");
    }

    request = objectStore.index("height").getAll();
    request.onerror = finishJSTest;
    request.onsuccess = continueWithEvent;
    event = yield;

    is(event.target.result instanceof Array, true, "Got an array object");
    is(event.target.result.length, objectStoreDataHeightSort.length, "Correct length");

    for (let i in event.target.result) {
        is(event.target.result[i], objectStoreDataHeightSort[i].value, "Correct value");
    }

    request = objectStore.index("height").getAll(null, 4);
    request.onerror = finishJSTest;
    request.onsuccess = continueWithEvent;
    event = yield;

    is(event.target.result instanceof Array, true, "Got an array object");
    is(event.target.result.length, 4, "Correct length");

    for (let i in event.target.result) {
        is(event.target.result[i], objectStoreDataHeightSort[i].value, "Correct value");
    }

    request = objectStore.index("height").getAll(65, 1);
    request.onerror = finishJSTest;
    request.onsuccess = continueWithEvent;
    event = yield;

    is(event.target.result instanceof Array, true, "Got an array object");
    is(event.target.result.length, 1, "Correct length");

    for (let i in event.target.result) {
        is(event.target.result[i], objectStoreDataHeightSort[parseInt(i) + 3].value, "Correct value");
    }

    finishJSTest();
    yield;
}

var testGenerator = testSteps();
testGenerator.next();
