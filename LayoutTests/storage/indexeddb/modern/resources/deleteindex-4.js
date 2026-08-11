description("This tests deleting an object store with an index, when aborting the transaction would *not* restore that object store, and makes sure the transaction successfully aborts");

var openRequest = null;
indexedDBTest(prepareDatabase);

function prepareDatabase(event)
{
    openRequest = event.target;
    tx = event.target.transaction;
    tx.onabort = function() {
        // Set event handler to null to avoid unexpected error message being printed.
        openRequest.onerror = null;
        endTestWithLog("Aborted!");
    }
    tx.onerror = function() {
        endTestWithLog("Unexpected error");
    }
    tx.oncomplete = function() {
        endTestWithLog("Unexpected completion");
    }

    db = event.target.result;
    db.createObjectStore("name");
    db.deleteObjectStore("name");
    
    os = db.createObjectStore("name");
    os.createIndex("index", "foo");
    os.put("bar", "foo").onsuccess = function() {
        debug("first put success");
        db.deleteObjectStore("name");
        db.createObjectStore("name").put("bar", "foo").onsuccess = function() {
            debug("second put success");
            tx.abort();
        }
    }
}
