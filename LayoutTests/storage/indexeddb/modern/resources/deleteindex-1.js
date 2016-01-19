description("This tests deleting an index and then committing the transaction.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var index;
var objectStore;
var database;

function checkIndexValues()
{
    var countRequest = index.count();
    countRequest.onsuccess = function() {
        debug("Count is: " + countRequest.result);
    }

    var cursorRequest = index.openCursor();
    cursorRequest.onsuccess = function() {
        var cursor = cursorRequest.result;
        if (!cursor) {
            objectStore.deleteIndex("TestIndex1");
            debug("Deleted the index");
            return;
        }

        debug("Cursor at record: " + cursor.key + " / " + cursor.primaryKey);
        cursor.continue();
    }
    cursorRequest.onerror = function(e) {
        debug("Unexpected error opening or iterating cursor");
        logCursor(cursorRequest.result);
        done();
    } 
}

var createRequest = window.indexedDB.open("DeleteIndex1Database", 1);
createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");
    objectStore.put({ bar: "A" }, 1);
    objectStore.put({ bar: "A" }, 2);

    index = objectStore.createIndex("TestIndex1", "bar");
    
    checkIndexValues();
    
    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected abort");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        continueTest();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function continueTest() {
    var transaction = database.transaction("TestObjectStore");
    objectStore = transaction.objectStore("TestObjectStore");

    var names = objectStore.indexNames;
    debug("Object store has indexes:")
    for (var i = 0; i < names.length; ++i)
        debug(names[i]);
    
    try {
        objectStore.index("TestIndex1");
    } catch(e) {
        debug("Unable to get index from object store (it shouldn't exist)");
    }

    transaction.onabort = function(event) {
        debug("Transaction unexpected abort");
        done();
    }

    transaction.oncomplete = function(event) {
        debug("Transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("Transaction unexpected error" + event);
        done();
    }
}
