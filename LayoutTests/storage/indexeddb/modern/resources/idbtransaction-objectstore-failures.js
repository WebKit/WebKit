description("This tests some obvious failures that can happen while calling transaction.objectStore()");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

function prepareDatabase(event)
{
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
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
        debug("First version change transaction abort");
        done();
    }

    tx.oncomplete = function(event) {
        debug("First version change transaction unexpected complete");
        done();
    }

    tx.onerror = function(event) {
        debug("First version change transaction unexpected error - " + event);
        done();
    }
}
