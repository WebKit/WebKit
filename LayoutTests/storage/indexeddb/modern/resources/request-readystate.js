description("This test makes sure that IDBRequest.readyState returns expected values.");
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

var createRequest = window.indexedDB.open("RequestReadyStateDatabase", 1);
debug("After calling indexedDB.open(), create request readyState is: " + createRequest.readyState);

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    debug("Create request readyState is: " + createRequest.readyState);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");

    for (var i = 0; i < 2; ++i)
        objectStore.put("Record " + i, i);
    
    var request = objectStore.openCursor();
    debug("After calling openCursor, request readyState is: " + request.readyState);
    request.onsuccess = function() {
        debug("After successful opening of the cursor, request readyState is: " + request.readyState);

        request.onsuccess = function() {
            debug("After continue() completed, request readyState is: " + request.readyState);
        }

        request.result.continue();
        debug("After calling continue(), request readyState is: " + request.readyState);
    }
          
    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        done();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}


