description("This tests deleting an index and then aborting the transaction.");

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
        if (!cursor)
            return;

        debug("Cursor at record: " + cursor.key + " / " + cursor.primaryKey);
        cursor.continue();
    }
    cursorRequest.onerror = function(e) {
        debug("Unexpected error opening or iterating cursor");
        logCursor(cursorRequest.result);
        done();
    } 
}

var createRequest = window.indexedDB.open("DeleteIndex2Database", 1);
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
        database.close();
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function continueTest1() {
    var createRequest = window.indexedDB.open("DeleteIndex2Database", 2);
    createRequest.onupgradeneeded = function(event) {
        debug("Second upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        database = event.target.result;
        var versionTransaction = createRequest.transaction;
        objectStore = versionTransaction.objectStore("TestObjectStore");
        objectStore.deleteIndex("TestIndex1");
        debug("Deleted the index");
        versionTransaction.abort();
        debug("Aborted the transaction");

        versionTransaction.onabort = function(event) {
            debug("Second upgrade versionchange transaction abort");
            database.close();
            continueTest2();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Second upgrade versionchange transaction unexpected complete");
            done();
        }

        versionTransaction.onerror = function(event) {
            debug("Second upgrade versionchange transaction error " + event);
        }
    }
}

function continueTest2() {
    var createRequest = window.indexedDB.open("DeleteIndex2Database", 3);
    createRequest.onupgradeneeded = function(event) {
        debug("Third upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        var versionTransaction = createRequest.transaction;
        objectStore = versionTransaction.objectStore("TestObjectStore");
        index = objectStore.index("TestIndex1");
    
        checkIndexValues();    
        
        versionTransaction.onabort = function(event) {
            debug("Third upgrade versionchange transaction unexpected abort");
            done();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Third upgrade versionchange transaction complete");
            done();
        }

        versionTransaction.onerror = function(event) {
            debug("Third upgrade versionchange transaction unexpected error" + event);
            done();
        }
    }
}
