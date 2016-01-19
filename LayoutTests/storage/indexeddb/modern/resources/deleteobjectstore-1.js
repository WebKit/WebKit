description("This test creates an object store then populates it, then commits that transaction. \
It then deletes it, but aborts that transaction. \
Finally it checks to make sure everything from step 1 is there as expected.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("DeleteObjectStore1Database", 1);

createRequest.onupgradeneeded = function(event) {
    debug("ALERT: " + "Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    for (var i = 0; i < 10; ++i)
        objectStore.put("AH AH AH AH AH", i + " puts");

    versionTransaction.onabort = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction complete");
        database.close();
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function getChecker(event) {
    debug("ALERT: " + "Value gotten was " + event.target.result);
}

function continueTest1()
{
    var openRequest = window.indexedDB.open("DeleteObjectStore1Database", 2);

    openRequest.onerror = function(event) {
        debug("ALERT: " + "Request error - " + event);
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
        var database = event.target.result;
        var objectStore = versionTransaction.objectStore("TestObjectStore");
        
        debug("ALERT: " + "Deleting object store");        
        database.deleteObjectStore("TestObjectStore");

        versionTransaction.abort();

        versionTransaction.onabort = function(event) {
            debug("ALERT: " + "Second version change transaction abort");
            continueTest2();
            database.close();
        }

        versionTransaction.oncomplete = function(event) {
            debug("ALERT: " + "Second version change transaction unexpected complete");
            done();
        }

        versionTransaction.onerror = function(event) {
            debug("ALERT: " + "Second version change transaction error - " + event);
        }
    }
}

function continueTest2()
{
    var openRequest = window.indexedDB.open("DeleteObjectStore1Database", 1);

    openRequest.onerror = function(event) {
        debug("ALERT: " + "Request unexpected error - " + event);
        done();
    }
    openRequest.onblocked = function(event) {
        debug("ALERT: " + "Request unexpected blocked - " + event);
        done();
    }
    openRequest.onupgradeneeded = function(event) {
        debug("ALERT: " + "Request unexpected upgradeneeded - " + event);
        done();
    }

    openRequest.onsuccess = function(event) {
        debug("ALERT: " + "Success opening database connection - Starting final transaction");
        var database = event.target.result;
        var transaction = database.transaction("TestObjectStore", "readwrite");
        var objectStore = transaction.objectStore("TestObjectStore");
    
        var request;
        for (var i = 0; i < 10; ++i) {
            request = objectStore.get(i + " puts");
            request.onsuccess = getChecker;
        }

        transaction.onabort = function(event) {
            debug("ALERT: " + "Final transaction unexpected abort");
            done();
        }

        transaction.oncomplete = function(event) {
            debug("ALERT: " + "Final transaction complete");
            done();
        }

        transaction.onerror = function(event) {
            debug("ALERT: " + "Final transaction unexpected error - " + event);
            done();
        }
    }
}
