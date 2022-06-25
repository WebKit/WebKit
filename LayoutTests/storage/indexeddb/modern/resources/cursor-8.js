description("This tests iterating a \"prev\" cursor in a read-write transaction while changing records.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");

    for (var i = 0; i < 3; ++i)
        objectStore.put("Record " + i, i);
    
    var request = objectStore.openCursor(IDBKeyRange.lowerBound(-Infinity), "prev");
    request.onsuccess = function() {
        debug("Cursor open at key " + request.result.key);

        // This tests deleting the current record out of underneath the cursor.
        // Its current key should be 0, after an iteration its next key should be 1.
        objectStore.clear().onsuccess = function() {
            debug("Object store cleared.");
        }
        
        // The cursor is currently on key 2, which no longer exists.
        // It's next key would have been 1, but the object store has been cleared.
        // Whatever key we put in the object store that is less than 2 will be the actual next key.
        objectStore.put("Record -100", -100);
    
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
