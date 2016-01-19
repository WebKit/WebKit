description("This tests some obvious failures that can happen while calling IDBDatabase.transaction()");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("IDBDatabaseTransactionFailuresDatabase");
var database;

createRequest.onupgradeneeded = function(event) {
    debug("ALERT: " + "Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("foo", "bar");

    request.onerror = function(event) {
        debug("ALERT: " + "put FAILED - " + event);
        done();
    }
    
    try {
        database.transaction("TestObjectStore", "readonly");
    } catch(e) {
        debug("ALERT: " + "Failed to start a transaction while a versionChange transaction was in progress - " + e);
    }

    versionTransaction.onabort = function(event) {
        debug("ALERT: " + "versionchange transaction aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("ALERT: " + "versionchange transaction completed");
        continueTest();
    }

    versionTransaction.onerror = function(event) {
        debug("ALERT: " + "versionchange transaction error'ed - " + event);
        done();
    }
}

function continueTest()
{
    try {
        database.transaction([], "readonly");
    } catch(e) {
        debug("ALERT: " + "Failed to start a transaction with an empty set of object stores - " + e);
    }

    try {
        database.transaction("NonexistentObjectStore", "readonly");
    } catch(e) {
        debug("ALERT: " + "Failed to start a transaction to a nonexistent object store - " + e);
    }

    try {
        database.transaction("TestObjectStore", "blahblah");
    } catch(e) {
        debug("ALERT: " + "Failed to start a transaction with an invalid mode - " + e);
    }

    try {
        database.transaction("TestObjectStore", "versionchange");
    } catch(e) {
        debug("ALERT: " + "Failed to explicitly start a versionchange transaction - " + e);
    }
    
    try {
        database.close();
        database.transaction("TestObjectStore", "readonly");
    } catch(e) {
        debug("ALERT: " + "Failed to explicitly start a transaction with the close pending flag set - " + e);
    }
    
    done();
}
