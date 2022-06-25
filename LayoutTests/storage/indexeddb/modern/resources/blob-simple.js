// Original implementation of test from Mozilla: ./dom/indexedDB/test/test_blob_simple.html
description("This tests basic operations putting blobs into an object store and then retrieving them.");

indexedDBTest(prepareDatabase);

function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var testGenerator;

function continueWithEvent(event)
{
    testGenerator.next(event);
}

function asyncContinue()
{
    setTimeout("testGenerator.next();", 0);
}

function idbRequest(request)
{
    request.onerror = continueWithEvent;
    request.onsuccess = continueWithEvent;
}

var db;

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    db = event.target.result;
    db.createObjectStore("foo", { autoIncrement: true }).createIndex("foo", "index");
    event.target.onsuccess = function() {
        testGenerator = testSteps();
        testGenerator.next();
    };
}

function is(a, b, message) {
    result = a == b ? "true" : "false";
    debug(message + ": " + result);
}

function* testSteps()
{
    debug("Let's create a blob and store it in IndexedDB twice.");

    const BLOB_DATA = ["fun ", "times ", "all ", "around!"];
    const INDEX_KEY = 5;
    let blob = new Blob(BLOB_DATA, { type: "text/plain" });
    let data = { blob: blob, index: INDEX_KEY };

    objectStore = db.transaction("foo", "readwrite").objectStore("foo");
    
    idbRequest(objectStore.add(data));
    event = yield;
    
    debug("Added blob to database once");

    let key1 = event.target.result;

    idbRequest(objectStore.add(data));
    event = yield;

    debug("Added blob to database twice");
    
    let key2 = event.target.result;

    debug("Let's retrieve the blob again and verify the contents is the same.");

    objectStore = db.transaction("foo").objectStore("foo");
    idbRequest(objectStore.get(key1));
    event = yield;

    debug("Got blob from database");
    
    let fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(event.target.result.blob);
    event = yield;

    is(event.target.result, BLOB_DATA.join(""), "Correct text");

    debug("Let's retrieve it again, create an object URL for the blob, load it via an XMLHttpRequest, and verify the contents are the same.");
    
    objectStore = db.transaction("foo").objectStore("foo");
    idbRequest(objectStore.get(key1));
    event = yield;
    
    debug("Got blob from database");
    
    let blobURL = URL.createObjectURL(event.target.result.blob);
    
    let xhr = new XMLHttpRequest();
    xhr.open("GET", blobURL);
    xhr.onload = continueWithEvent;
    xhr.send();
    yield;
    
    URL.revokeObjectURL(blobURL);    
    is(xhr.responseText, BLOB_DATA.join(""), "Correct responseText");
    
    debug("Retrieve both blob entries from the database and verify contents.");

    var objects = []
    
    objectStore = db.transaction("foo").objectStore("foo");
    idbRequest(objectStore.get(key1));
    event = yield;
    
    objects.push(event.target.result);
    
    objectStore = db.transaction("foo").objectStore("foo");
    idbRequest(objectStore.get(key2));
    event = yield;
    
    objects.push(event.target.result);
        
    fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(objects[0].blob);
    event = yield;

    is(event.target.result, BLOB_DATA.join(""), "Correct text on first value");

    fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(objects[1].blob);
    event = yield;

    is(event.target.result, BLOB_DATA.join(""), "Correct text on second value");

    let cursorResults = [];
    
    objectStore = db.transaction("foo").objectStore("foo");
    objectStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        debug("Got item from cursor");
        cursorResults.push(cursor.value);
        cursor.continue();
      }
      else {
        debug("Finished cursor");
        asyncContinue();
      }
    };
    yield;

    is(cursorResults.length, 2, "Got right number of items");

    fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(cursorResults[0].blob);
    event = yield;
    
    is(event.target.result, BLOB_DATA.join(""), "Correct text");

    debug("Retrieve blobs from database via index and verify contents.");
    
    index = db.transaction("foo").objectStore("foo").index("foo");
    idbRequest(index.get(INDEX_KEY)); 
    event = yield;
    
    debug("Got blob from database");
    
    fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(event.target.result.blob);
    event = yield;
    
    is(event.target.result, BLOB_DATA.join(""), "Correct text");
        
    cursorResults = [];
    
    index = db.transaction("foo").objectStore("foo").index("foo");
    index.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        debug("Got item from cursor");
        cursorResults.push(cursor.value);
        cursor.continue();
      }
      else {
        debug("Finished cursor");
        asyncContinue();
      }
    };
    yield;
    
    is(cursorResults.length, 2, "Got right number of items");
    
    fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(cursorResults[0].blob);
    event = yield;
    
    is(event.target.result, BLOB_DATA.join(""), "Correct text");
    
    fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(cursorResults[1].blob);
    event = yield;
    
    is(event.target.result, BLOB_DATA.join(""), "Correct text");
    
    debug("Slice the the retrieved blob and verify its contents.");
    
    let slice = cursorResults[1].blob.slice(0, BLOB_DATA[0].length);
    
    fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(slice);
    event = yield;
    
    is(event.target.result, BLOB_DATA[0], "Correct text");
    
    debug("Send blob to a worker, read its contents there, and verify results.");
    
    function workerScript() {
        onmessage = function(event) {
            var reader = new FileReaderSync();
            postMessage(reader.readAsText(event.data));
    
            var slice = event.data.slice(1, 2);
            postMessage(reader.readAsText(slice));
        }
    }
    
    let url = URL.createObjectURL(new Blob(["(", workerScript, ")()"]));
    
    let worker = new Worker(url);
    worker.postMessage(slice);
    worker.onmessage = continueWithEvent;
    event = yield;
    
    is(event.data, BLOB_DATA[0], "Correct text");
    event = yield;

    is(event.data, BLOB_DATA[0][1], "Correct text");
    
    debug("Store a blob back in the database, and keep holding on to the " +
         "blob, verifying that it still can be read.");
    
    objectStore = db.transaction("foo").objectStore("foo");
    idbRequest(objectStore.get(key1)); 
    event = yield;
    
    let blobFromDB = event.target.result.blob;
    debug("Got blob from database");
    
    let txn = db.transaction("foo", "readwrite");
    txn.objectStore("foo").put(event.target.result, key1);
    txn.oncomplete = continueWithEvent;
    event = yield;
    
    debug("Stored blob back into database");
    
    fileReader = new FileReader();
    fileReader.onload = continueWithEvent;
    fileReader.readAsText(blobFromDB);
    event = yield;
    
    is(event.target.result, BLOB_DATA.join(""), "Correct text");
    
    blobURL = URL.createObjectURL(blobFromDB);
    
    xhr = new XMLHttpRequest();
    xhr.open("GET", blobURL);
    xhr.onload = continueWithEvent;
    xhr.send();
    yield;
    
    URL.revokeObjectURL(blobURL);
    
    is(xhr.responseText, BLOB_DATA.join(""), "Correct responseText");
    
    finishJSTest();
 }
 