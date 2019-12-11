description("This tests deleting an object store with an index, when aborting the transaction would *not* restore that object store, and makes sure the transaction successfully aborts");

indexedDBTest(prepareDatabase);

function prepareDatabase(event)
{
    tx = event.target.transaction;
    tx.onabort = function() {
        debug("Aborted!");
        finishJSTest();
    }
    tx.onerror = function() {
        debug("Unexpected error");
        finishJSTest();
    }
    tx.oncomplete = function() {
        debug("Unexpected completion");
        finishJSTest();
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
