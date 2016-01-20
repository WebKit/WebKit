description("This test creates an object store then populates it, then commits that transaction. \
It then clears it, but aborts that transaction. \
Finally it checks to make sure everything from step 1 is still in there.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("IDBObjectStoreClear2Database", 1);

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore", { autoIncrement: true });
    var request = objectStore.put("bar1");
    var request = objectStore.put("bar2");
    var request = objectStore.put("bar3");
    var request = objectStore.put("bar4");
    var request = objectStore.put("bar5");
    var request = objectStore.put("bar6");
    var request = objectStore.put("bar7");
    var request = objectStore.put("bar8");
    var request = objectStore.put("bar9");

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        continueTest1();
        database.close();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function getChecker(event) {
    debug("Value gotten was " + event.target.result);
}

function continueTest1()
{
    var openRequest = window.indexedDB.open("IDBObjectStoreClear2Database", 1);

    openRequest.onerror = function(event) {
        debug("Request unexpected error - " + event);
        done();
    }
    openRequest.onblocked = function(event) {
        debug("Request unexpected blocked - " + event);
        done();
    }
    openRequest.onupgradeneeded = function(event) {
        debug("Request unexpected upgradeneeded - " + event);
        done();
    }

    openRequest.onsuccess = function(event) {
        debug("Success opening database connection - Starting readwrite transaction");
        var database = event.target.result;
        var transaction = database.transaction("TestObjectStore", "readwrite");
        var objectStore = transaction.objectStore("TestObjectStore");
    
        var request;
        for (var i = 1; i <= 9; ++i) {
            request = objectStore.get(i);
            request.onsuccess = getChecker;
        }
        
        request = objectStore.clear();
        request.onsuccess = function() {
            debug("Object store cleared, making sure its all gone.");
            var newRequests;
            for (var i = 1; i <= 9; ++i) {
                newRequests = objectStore.get(i);
                newRequests.onsuccess = getChecker;
            }
            
            // We'll be on the last request here, can just override it.
            newRequests.onsuccess = function(event) {
                getChecker(event);
                transaction.abort();
            }
        }
        
        // Just for fun clear it twice, won't change anything
        objectStore.clear();

        transaction.onabort = function(event) {
            debug("Readwrite transaction abort");
            continueTest2();
        }

        transaction.oncomplete = function(event) {
            debug("Readwrite transaction unexpected complete");
            done();
        }

        transaction.onerror = function(event) {
            debug("Readwrite transaction unexpected error - " + event);
            done();
        }
    }
}

function continueTest2()
{
    var openRequest = window.indexedDB.open("IDBObjectStoreClear2Database", 1);

    openRequest.onerror = function(event) {
        debug("Request unexpected error - " + event);
        done();
    }
    openRequest.onblocked = function(event) {
        debug("Request unexpected blocked - " + event);
        done();
    }
    openRequest.onupgradeneeded = function(event) {
        debug("Request unexpected upgradeneeded - " + event);
        done();
    }

    openRequest.onsuccess = function(event) {
        debug("Success opening database connection - Starting final transaction");
        var database = event.target.result;
        var transaction = database.transaction("TestObjectStore", "readwrite");
        var objectStore = transaction.objectStore("TestObjectStore");
    
        var request;
        for (var i = 1; i <= 9; ++i) {
            request = objectStore.get(i);
            request.onsuccess = getChecker;
        }

        transaction.onabort = function(event) {
            debug("Readwrite transaction unexpected abort");
            done();
        }

        transaction.oncomplete = function(event) {
            debug("Readwrite transaction complete");
            done();
        }

        transaction.onerror = function(event) {
            debug("Readwrite transaction unexpected error - " + event);
            done();
        }
    }
}
