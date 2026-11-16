description("This tests some obvious failures that can happen while calling IDBDatabase.transaction()");

indexedDBTest(prepareDatabase);

var database;

function prepareDatabase(event)
{
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    // Set error event handler to null to avoid unexpected error message being printed.
    event.target.onerror = null;

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("foo", "bar");

    request.onerror = function(event) {
        endTestWithLog("put FAILED - " + event);
    }
    
    try {
        database.transaction("TestObjectStore", "readonly");
    } catch(e) {
        debug("Failed to start a transaction while a versionChange transaction was in progress - " + e);
    }

    versionTransaction.onabort = function(event) {
        endTestWithLog("versionchange transaction aborted");
    }

    versionTransaction.oncomplete = function(event) {
        debug("versionchange transaction completed");
        continueTest();
    }

    versionTransaction.onerror = function(event) {
        endTestWithLog("versionchange transaction error'ed - " + event);
    }
}

function continueTest()
{
    try {
        database.transaction([], "readonly");
    } catch(e) {
        debug("Failed to start a transaction with an empty set of object stores - " + e);
    }

    try {
        database.transaction("NonexistentObjectStore", "readonly");
    } catch(e) {
        debug("Failed to start a transaction to a nonexistent object store - " + e);
    }

    try {
        database.transaction("TestObjectStore", "blahblah");
    } catch(e) {
        debug("Failed to start a transaction with an invalid mode - " + e);
    }

    try {
        database.transaction("TestObjectStore", "versionchange");
    } catch(e) {
        debug("Failed to explicitly start a versionchange transaction - " + e);
    }
    
    try {
        database.close();
        database.transaction("TestObjectStore", "readonly");
    } catch(e) {
        debug("Failed to explicitly start a transaction with the close pending flag set - " + e);
    }
    
    finishJSTest();
}
