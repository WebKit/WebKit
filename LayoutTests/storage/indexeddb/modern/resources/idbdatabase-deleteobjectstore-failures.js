description("This tests some obvious failures that can happen while calling IDBDatabase.deleteObjectStore()");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("IDBDatabaseDeleteObjectStoreFailuresDatabase", 1);
var database;

createRequest.onupgradeneeded = function(event) {
    debug("ALERT: " + "Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("bar", "foo");

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
    var transaction = database.transaction("TestObjectStore", "readwrite");
    var objectStore = transaction.objectStore("TestObjectStore");
    var request = objectStore.put("baz", "foo");

    request.onsuccess = function() {
        debug("ALERT: " + "readwrite put success - about to try to delete an objectstore");
        try {
            database.deleteObjectStore("TestObjectStore");
        } catch(e) {
            debug("ALERT: " + "Failed to deleteObjectStore without a versionchange transaction - " + e);
        }
    }
    
    transaction.onabort = function(event) {
        debug("ALERT: " + "readwrite transaction unexpected aborted");
        done();
    }

    transaction.oncomplete = function(event) {
        debug("ALERT: " + "readwrite transaction complete");
        database.close();
        continueTest2();
    }

    transaction.onerror = function(event) {
        debug("ALERT: " + "readwrite transaction unexpected error" + event);
        done();
    }
}

function continueTest2()
{
    var openRequest = window.indexedDB.open("IDBDatabaseDeleteObjectStoreFailuresDatabase", 2);

    openRequest.onerror = function(event) {
        debug("ALERT: " + "Request unexpected error - " + event);
        done();
    }
    openRequest.onblocked = function(event) {
        debug("ALERT: " + "Request unexpected blocked - " + event);
        done();
    }
    openRequest.onsuccess = function(event) {
        debug("ALERT: " + "Request unexpected success - " + event);
        done();
    }

    openRequest.onupgradeneeded = function(event) {
        debug("ALERT: " + "Second upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
        var versionTransaction = openRequest.transaction;
        database = event.target.result;

        try {
            database.deleteObjectStore("NonexistentObjectStore");
        } catch(e) {
            debug("ALERT: " + "Failed to deleteObjectStore with a non-existent objectstore - " + e);
        }

        // Spin the transaction with get requests to keep it alive long enough for the setTimeout to fire.
        var objectStore = versionTransaction.objectStore("TestObjectStore");
        var canFinish = false;
        var spinGet = function() { 
            objectStore.get("foo").onsuccess = function() {
                if (!canFinish)
                    spinGet();
            }
        }
        spinGet();
        
        // After the versionChange transaction becomes inactive, but while it's still in-progress, try to delete the objectstore
        var tryInactiveDelete = function() 
        {
            try {
                database.deleteObjectStore("TestObjectStore");
            } catch(e) {
                debug("ALERT: " + "Failed to deleteObjectStore with an in-progress versionchange transaction that is inactive - " + e);
            }
            canFinish = true;
        }
        
        setTimeout(tryInactiveDelete, 0);

        versionTransaction.onabort = function(event) {
            debug("ALERT: " + "Second version change transaction unexpected abort");
            done();
        }

        versionTransaction.oncomplete = function(event) {
            debug("ALERT: " + "Second version change transaction complete");
            done();
        }

        versionTransaction.onerror = function(event) {
            debug("ALERT: " + "Second version change transaction unexpected error - " + event);
            done();
        }
    }
}
