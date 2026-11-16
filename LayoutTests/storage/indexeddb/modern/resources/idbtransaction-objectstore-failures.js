description("This tests some obvious failures that can happen while calling transaction.objectStore()");

indexedDBTest(prepareDatabase);

function prepareDatabase(event)
{
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    // Set error event handler to null to avoid unexpected error message being printed.
    event.target.onerror = null;

    var tx = event.target.transaction;
    var db = event.target.result;

    debug(tx + " - " + tx.mode);
    debug(db);

    var os1 = db.createObjectStore("TestObjectStore1");
    var os2 = db.createObjectStore("TestObjectStore2");

    var putRequest = os1.put("bar", "foo");
    
    putRequest.onerror = function() {
        debug("put failed (because transaction was aborted)");
    }
    
    try {
        tx.objectStore("");
    } catch(e) {
        debug("Caught attempt to access empty-named object store on the transaction");
    }
    
    try {
        tx.objectStore();
    } catch(e) {
        debug("Caught attempt to access null-named object store on the transaction");
    }
     
    try {
        tx.objectStore("ThisObjectStoreDoesNotExist");
    } catch(e) {
        debug("Caught attempt to access non-existant object store on the transaction");
    }
    
    tx.abort();
    
    try {
        tx.objectStore("TestObjectStore1");
    } catch(e) {
        debug("Caught attempt to access valid object store on a transaction that is already finishing");
    }
      
    tx.onabort = function(event) {
        endTestWithLog("First version change transaction abort");
    }

    tx.oncomplete = function(event) {
        endTestWithLog("First version change transaction unexpected complete");
    }

    tx.onerror = function(event) {
        endTestWithLog("First version change transaction unexpected error - " + event);
    }
}
