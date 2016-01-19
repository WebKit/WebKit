description("This tests some obvious failures that can happen while calling IDBObjectStore.createIndex().");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var createRequest = window.indexedDB.open("IDBObjectStoreCreateIndexFailuresDatabase", 1);
var database;

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");

    try {
        objectStore.createIndex(null, "foo");
    } catch(e) {
        debug("Failed to create index with null name");
    }
    
    try {
        objectStore.createIndex("TestIndex1", null);
    } catch(e) {
        debug("Failed to create index with invalid key path");
    }
    
    database.deleteObjectStore("TestObjectStore");
    try {
        objectStore.createIndex("TestIndex2", "foo");
    } catch(e) {
        debug("Failed to create index on a deleted object store");
    }

    objectStore = database.createObjectStore("TestObjectStore");
    
    try {
        objectStore.createIndex("TestIndex3", ["foo", "bar"], { multiEntry: true });
    } catch(e) {
        debug("Failed to create multi-entry index with an array key path");
    }
    
    objectStore.createIndex("TestIndex4", "foo");
    try {
        objectStore.createIndex("TestIndex4", "foo");
    } catch(e) {
        debug("Failed to create index that already exists");
    }

    // Spin the transaction with get requests to keep it alive long enough for the setTimeout to fire.
    var canFinish = false;
    var spinGet = function() { 
        objectStore.get("foo").onsuccess = function() {
            if (!canFinish)
                spinGet();
        }
    }
    spinGet();

    var createWhileInactive = function() {
        try {
            objectStore.createIndex("TestIndex5", "foo");
        } catch(e) {
            debug("Failed to create index while the transaction is inactive");
        }
        canFinish = true;
    }
    
    setTimeout(createWhileInactive, 0);
    
    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function continueTest1()
{
    var transaction = database.transaction("TestObjectStore", "readwrite");
    var objectStore = transaction.objectStore("TestObjectStore");

    try {
        objectStore.createIndex("TestIndex6", "foo");
    } catch(e) {
        debug("Failed to create index outside of a version change transaction");
    }
        
    transaction.onabort = function(event) {
        debug("readonly transaction unexpected abort" + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readonly transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readonly transaction unexpected error" + event);
        done();
    }
}
