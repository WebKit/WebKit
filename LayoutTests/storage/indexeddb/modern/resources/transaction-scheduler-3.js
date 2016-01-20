description("This test makes sure that a write transaction is blocked by a read transaction with overlapping scope. It also makes sure the write transaction starts after the read transaction completes.");


if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("TransactionScheduler3Database");

createRequest.onupgradeneeded = function(event) {
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("OS");
    var request = objectStore.put("bar", "foo");

    request.onerror = function(event) {
        debug("put FAILED - " + event);
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
function continueTest()
{
    var longRunningReadRequest = window.indexedDB.open("TransactionScheduler3Database", 1);
    longRunningReadRequest.onsuccess = function(event) {
        debug("Success opening database connection - Starting readonly transaction");
        secondDatabaseConnection = event.target.result;
        readTransactionLoop(secondDatabaseConnection.transaction("OS", "readonly"), true);
    }
    longRunningReadRequest.onerror = function(event) {
        debug("Long running read request unexpected error - " + event);
        done();
    }
    longRunningReadRequest.onblocked = function(event) {
        debug("Long running read request unexpected blocked - " + event);
        done();
    }
    longRunningReadRequest.onupgradeneeded = function(event) {
        debug("Long running read request unexpected upgradeneeded - " + event);
        done();
    } 
}

var shouldEndReadTransaction = false;

function readTransactionLoop(transaction, isFirstTime)
{
    var objectStore = transaction.objectStore("OS");
    var request = objectStore.get("foo");
    
    request.onsuccess = function(event) {
        if (!shouldEndReadTransaction)
            readTransactionLoop(event.target.transaction, false);
        
        // Now that the read transaction is open, the write transaction can be started, but it will be blocked to start with.
        if (isFirstTime)
             startWriteTransaction()
    }

    request.onerror = function(event) {
        debug("Unexpected request error - " + event);
        done();
    }

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
}

function startWriteTransaction()
{
    debug("Creating write transaction");
    var transaction = secondDatabaseConnection.transaction("OS", "readwrite");
    var objectStore = transaction.objectStore("OS");
    var request = objectStore.put("baz", "foo");

    setTimeout("shouldEndReadTransaction = true;", 0);

    request.onsuccess = function(event) {
        debug("Write transaction put success");
    }

    request.onerror = function(event) {
        debug("Write transaction put unexpected error - " + event);
        done();
    }

    transaction.onerror = function(event) {
        debug("Write transaction unexpected error - " + event);
        done();
    }

    transaction.onabort = function(event) {
        debug("Write transaction unexpected abort - " + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("Write transaction complete - " + event);
        done();
    }
}


