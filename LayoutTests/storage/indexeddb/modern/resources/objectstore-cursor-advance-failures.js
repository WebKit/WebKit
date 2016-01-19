description("This tests some obvious failures that can happen while calling IDBCursor.advance() on object store cursors.");
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

var createRequest = window.indexedDB.open("IDBAdvanceFailuresDatabase", 1);
var database;

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    for (var i = 0; i < 10; ++i)
        objectStore.put("Record " + i, i);
    
    var request = objectStore.openCursor();
    request.onsuccess = function() {
        var cursor = request.result;

        try {
            cursor.advance();
        } catch(e) {
            debug("Failed to advance object store cursor with undefined count argument");
        }

        try {
            cursor.advance(0);
        } catch(e) {
            debug("Failed to advance object store cursor with '0' count argument");
        }

        try {
            cursor.advance(-1);
        } catch(e) {
            debug("Failed to advance object store cursor with negative count argument");
        }

        database.deleteObjectStore("TestObjectStore");
        try {
            cursor.advance(1);
        } catch(e) {
            debug("Failed to advance object store cursor after deleting object store");
        }

        // Recreate the object store for use in the next stage of testing
        objectStore = database.createObjectStore("TestObjectStore");
        for (var i = 0; i < 10; ++i)
            objectStore.put("Record " + i, i);
    }
    
    var os2 = database.createObjectStore("TestObjectStore2");
    for (var i = 0; i < 10; ++i)
        os2.put("Record " + i, i);
        
    var request2 = os2.openCursor();
    request2.onsuccess = function() {
        var cursor = request2.result;
        cursor.continue();
        try {
            cursor.advance(1);
        } catch(e) {
            debug("Failed to advance an object store cursor when it is already fetching the next record");
        }
        request2.onsuccess = undefined;
    }

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function continueTest1()
{
    var transaction = database.transaction("TestObjectStore", "readonly");
    var objectStore = transaction.objectStore("TestObjectStore");
    var cursor = objectStore.openCursor();

    // Spin the transaction with get requests to keep it alive long enough for the setTimeout to fire.
    var canFinish = false;
    var spinGet = function() { 
        objectStore.get("foo").onsuccess = function() {
            if (!canFinish)
                spinGet();
        }
    }
    spinGet();

    var testWhileInactive = function() {
        try {
            cursor.advance(1);
        } catch(e) {
            debug("Failed to advance object store cursor while transaction inactive");
        }
        canFinish = true;
    }
    
    setTimeout(testWhileInactive, 0);
    
    transaction.onabort = function(event) {
        debug("readonly transaction unexpected abort" + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readonly transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readonly transaction unexpected error" + event);
        done();
    }
}


