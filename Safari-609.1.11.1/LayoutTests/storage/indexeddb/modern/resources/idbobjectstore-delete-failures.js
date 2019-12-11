description("This tests some obvious failures that can happen while calling IDBObjectStore.delete().");

indexedDBTest(prepareDatabase);


function log(message)
{
    debug(message);
}

function done()
{
    finishJSTest();
}

var database;

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("bar", "foo");

    request.onsuccess = function() {
        try {
            objectStore.delete(NaN);
        } catch(e) {
            debug("Failed to delete record from object store with an invalid key");
        }
        
        database.deleteObjectStore("TestObjectStore");
        
        try {
            objectStore.delete("foo");
        } catch(e) {
            debug("Failed to delete record from object store that has been deleted");
        } 

        // Recreate the objectstore because we'll need it in phase 2.
        var objectStore = database.createObjectStore("TestObjectStore");
        objectStore.put("bar", "foo");    
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
    
    try {
        objectStore.delete("foo");
    } catch(e) {
        debug("Failed to delete a record in read-only transaction");
    }
    
    var transaction = database.transaction("TestObjectStore", "readwrite");
    var objectStore = transaction.objectStore("TestObjectStore");

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
            objectStore.delete("foo");
        } catch(e) {
            debug("Failed to delete record with inactive transaction");
        }
        canFinish = true;
    }
    
    setTimeout(testWhileInactive, 0);
    
    transaction.onabort = function(event) {
        debug("readwrite transaction unexpected abort" + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readwrite transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readwrite transaction unexpected error" + event);
        done();
    }
}


