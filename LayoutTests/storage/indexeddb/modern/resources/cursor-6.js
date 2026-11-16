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
        // Its current key should be 2, after an iteration its next key should be 1.
        objectStore.delete(2).onsuccess = function() {
            debug("Record 2 deleted, even though that's where the cursor currently points.");
        }
        
        // Now that cursor iteration has begun, manually delete and then replace a record 
        // that will eventually be iterated to, making sure the new value is picked up. 
        objectStore.delete(0);
        objectStore.put("Scary new actual record!", 0);
    
        request.onsuccess = function() {
            debug("Cursor iterated to key " + request.result.key);
            
            request.onsuccess = function() {
                debug("Cursor iterated to key " + request.result.key + " with value '" + request.result.value + "'");
            }
            
            request.result.continue();
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
