description("This test creates two object stores. \
It then opens two long running read-only transactions, one per object store. \
It then opens a read-write transaction to both object stores. \
The read-only transactions both need to finish before the read-write transaction starts.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

var dbname;
function prepareDatabase(event)
{
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    event.target.onerror = null;

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    dbname = database.name;
    var objectStore = database.createObjectStore("OS1");
    var request = objectStore.put("bar", "foo");

    request.onerror = function(event) {
        debug("first put FAILED - " + event);
        done();
    }

    objectStore = database.createObjectStore("OS2");
    request = objectStore.put("bar", "foo");
    
    request.onerror = function(event) {
        debug("second put FAILED - " + event);
        done();
    }
    
    versionTransaction.onabort = function(event) {
        debug("versionchange transaction aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("versionchange transaction completed");
        continueTest();
        database.close();
    }

    versionTransaction.onerror = function(event) {
        debug("versionchange transaction error'ed - " + event);
        done();
    }
}

var secondDatabaseConnection;
var readWriteTransaction;

function setupReadTransactionCallbacks(transaction)
{
    transaction.onerror = function(event) {
        debug("Unexpected transaction error - " + event);
        done();
    }

    transaction.onabort = function(event) {
        debug("Unexpected transaction abort - " + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("Read transaction complete - " + event);
    }

    transaction.hasDoneFirstRead = false;
}

function continueTest()
{
    var openDBRequest = window.indexedDB.open(dbname, 1);
    openDBRequest.onsuccess = function(event) {
        debug("Success opening database connection - Starting transactions");
        secondDatabaseConnection = event.target.result;
        
        var transaction1 = secondDatabaseConnection.transaction("OS1", "readonly");
        setupReadTransactionCallbacks(transaction1);
        var transaction2 = secondDatabaseConnection.transaction("OS2", "readonly");
        setupReadTransactionCallbacks(transaction2);

        readTransactionLoop(transaction1, "OS1", false);
        readTransactionLoop(transaction2, "OS2", true);
    }
    openDBRequest.onerror = function(event) {
        debug("Long running read request unexpected error - " + event);
        done();
    }
    openDBRequest.onblocked = function(event) {
        debug("Long running read request unexpected blocked - " + event);
        done();
    }
    openDBRequest.onupgradeneeded = function(event) {
        debug("Long running read request unexpected upgradeneeded - " + event);
        done();
    } 
}

var shouldEndReadTransaction = false;

function readTransactionLoop(transaction, osname, shouldStartWrite)
{
    var objectStore = transaction.objectStore(osname);
    var request = objectStore.get("foo");
    
    request.onsuccess = function(event) {
        if (shouldEndReadTransaction && transaction.hasDoneFirstRead)
            return;

        transaction.hasDoneFirstRead = true;
        readTransactionLoop(event.target.transaction, osname, false);
        
        if (shouldStartWrite)
             startWriteTransaction();
    }

    request.onerror = function(event) {
        debug("Unexpected request error - " + event);
        done();
    }
}

function startWriteTransaction()
{
    debug("Starting write transaction");
    var transaction = secondDatabaseConnection.transaction(["OS1", "OS2"], "readwrite");
    var objectStore = transaction.objectStore("OS1");
    var request = objectStore.put("baz", "foo");

    shouldEndReadTransaction = true;

    request.onsuccess = function(event) {
        debug("Write to OS1 successful");
    }
    
    request.onerror = function(event) {
        debug("Write transaction put unexpected error - " + event);
        done();
    }

    objectStore = transaction.objectStore("OS2");
    request = objectStore.put("baz", "foo");

    request.onsuccess = function(event) {
        debug("Write to OS2 successful");
        done();
    }
    
    request.onerror = function(event) {
        debug("Write transaction put unexpected error - " + event);
        done();
    }
}


