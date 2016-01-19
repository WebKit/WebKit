description("This tests iterating a \"next\" cursor in a read-write transaction combined with clearing the object store.");

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

var createRequest = window.indexedDB.open("Cursor7Database", 1);

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");

    for (var i = 0; i < 3; ++i)
        objectStore.put("Record " + i, i);
    
    var request = objectStore.openCursor();
    request.onsuccess = function() {
        debug("Cursor open at key " + request.result.key);

        // This tests deleting the current record out of underneath the cursor.
        // Its current key should be 0, after an iteration its next key should be 1.
        objectStore.clear().onsuccess = function() {
            debug("Object store cleared.");
        }
        
        // The cursor is currently on key 0, which no longer exists.
        // It's next key would have been 1, but the object store has been cleared.
        // Whatever key we put in the object store that is greater than 0 will be the actual next key.
        objectStore.put("Record 100", 100);
    
        request.onsuccess = function() {
            debug("Cursor iterated to key " + request.result.key);
        }
        request.result.continue();
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
