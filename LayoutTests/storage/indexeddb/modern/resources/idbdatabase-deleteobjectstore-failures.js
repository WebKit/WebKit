description("This tests some obvious failures that can happen while calling IDBDatabase.deleteObjectStore()");

indexedDBTest(prepareDatabase);

var database;
var dbname;
function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    dbname = database.name;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("bar", "foo");

    versionTransaction.onabort = function(event) {
        endTestWithLog("Initial upgrade versionchange transaction unexpected aborted");
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        endTestWithLog("Initial upgrade versionchange transaction unexpected error" + event);
    }
}

function continueTest1()
{
    var transaction = database.transaction("TestObjectStore", "readwrite");
    var objectStore = transaction.objectStore("TestObjectStore");
    var request = objectStore.put("baz", "foo");

    request.onsuccess = function() {
        debug("readwrite put success - about to try to delete an objectstore");
        try {
            database.deleteObjectStore("TestObjectStore");
        } catch(e) {
            debug("Failed to deleteObjectStore without a versionchange transaction - " + e);
        }
    }
    
    transaction.onabort = function(event) {
        endTestWithLog("readwrite transaction unexpected aborted");
    }

    transaction.oncomplete = function(event) {
        debug("readwrite transaction complete");
        database.close();
        continueTest2();
    }

    transaction.onerror = function(event) {
        endTestWithLog("readwrite transaction unexpected error" + event);
    }
}

function continueTest2()
{
    var openRequest = window.indexedDB.open(dbname, 2);

    openRequest.onerror = function(event) {
        endTestWithLog("Request unexpected error - " + event);
    }
    openRequest.onblocked = function(event) {
        endTestWithLog("Request unexpected blocked - " + event);
    }
    openRequest.onsuccess = function(event) {
        endTestWithLog("Request unexpected success - " + event);
    }

    openRequest.onupgradeneeded = function(event) {
        debug("Second upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
        var versionTransaction = event.target.transaction;
        database = event.target.result;

        try {
            database.deleteObjectStore("NonexistentObjectStore");
        } catch(e) {
            debug("Failed to deleteObjectStore with a non-existent objectstore - " + e);
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
                debug("Failed to deleteObjectStore with an in-progress versionchange transaction that is inactive - " + e);
            }
            canFinish = true;
        }
        
        setTimeout(tryInactiveDelete, 0);

        versionTransaction.onabort = function(event) {
            endTestWithLog("Second version change transaction unexpected abort");
        }

        versionTransaction.oncomplete = function(event) {
            endTestWithLog("Second version change transaction complete");
        }

        versionTransaction.onerror = function(event) {
            endTestWithLog("Second version change transaction unexpected error - " + event);
        }
    }
}
