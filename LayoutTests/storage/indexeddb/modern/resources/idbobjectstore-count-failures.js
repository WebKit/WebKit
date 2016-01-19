description("This tests some obvious failures that can happen while calling IDBObjectStore.count().");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("IDBObjectStoreCountFailuresDatabase", 1);
var database;

createRequest.onupgradeneeded = function(event) {
    debug("ALERT: " + "Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("bar", "foo");

    request.onsuccess = function() {
        try {
            objectStore.count(NaN);
        } catch(e) {
            debug("ALERT: " + "Failed to count records in object store with an invalid key");
        }
        
        database.deleteObjectStore("TestObjectStore");
        
        try {
            objectStore.count();
        } catch(e) {
            debug("ALERT: " + "Failed to count records in object store that's been deleted");
        } 

        // Recreate the objectstore because we'll need it in phase 2.
        var objectStore = database.createObjectStore("TestObjectStore");
        objectStore.put("bar", "foo");    
    }
    
    versionTransaction.onabort = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction complete");
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function continueTest1()
{
    var transaction = database.transaction("TestObjectStore", "readonly");
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
    
    var getWhileInactive = function() {
        try {
            objectStore.count();
        } catch(e) {
            debug("ALERT: " + "Failed to count records in object store while transaction is inactive");
        }
        canFinish = true;
    }
    
    setTimeout(getWhileInactive, 0);
    
    transaction.onabort = function(event) {
        debug("ALERT: " + "readonly transaction unexpected abort" + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("ALERT: " + "readonly transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("ALERT: " + "readonly transaction unexpected error" + event);
        done();
    }
}


