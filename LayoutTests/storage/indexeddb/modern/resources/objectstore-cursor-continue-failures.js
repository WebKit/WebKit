description("This tests some obvious failures that can happen while calling IDBCursor.continue() on object store cursors.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var database;

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    for (var i = 0; i < 10; ++i)
        objectStore.put("Record " + i, i);
    
    var request = objectStore.openCursor();
    request.onsuccess = function() {
        var cursor = request.result;

        try {
            cursor.continue(-1);
        } catch(e) {
            debug("Failed to continue a 'next' object store cursor to a key less than the current key");
        }

        try {
            cursor.continue(NaN);
        } catch(e) {
            debug("Failed to continue object store cursor with invalid key");
        }

        database.deleteObjectStore("TestObjectStore");
        try {
            cursor.continue();
        } catch(e) {
            debug("Failed to continue object store cursor after deleting object store");
        }

        // Recreate the object store for use in the next stage of testing
        objectStore = database.createObjectStore("TestObjectStore");
        for (var i = 0; i < 10; ++i)
            objectStore.put("Record " + i, i);
    }
    
    var request2 = objectStore.openCursor(IDBKeyRange.lowerBound(-Infinity), "prev");
    request2.onsuccess = function() {
        var cursor = request2.result;
        try {
            cursor.continue(100);
        } catch(e) {
            debug("Failed to continue a 'prev' object store cursor to a key greater than the current key");
        }
    }

    var os2 = database.createObjectStore("TestObjectStore2");
    for (var i = 0; i < 10; ++i)
        os2.put("Record " + i, i);
        
    var request3 = os2.openCursor();
    request3.onsuccess = function() {
        var cursor = request3.result;
        cursor.continue();
        try {
            cursor.continue();
        } catch(e) {
            debug("Failed to continue an object store cursor when it is already fetching the next record");
        }
        request3.onsuccess = undefined;
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
            cursor.continue();
        } catch(e) {
            debug("Failed to continue object store cursor while transaction inactive");
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


