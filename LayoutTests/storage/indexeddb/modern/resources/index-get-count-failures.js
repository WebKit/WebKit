description("This tests some obvious failures that can happen while calling the IDBIndex methods get(), getKey(), and count().");
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

var createRequest = window.indexedDB.open("IndexGetCountFailuresDatabase", 1);
var database;
var index;

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    index = objectStore.createIndex("TestIndex", "foo");
    
    try {
        index.get(null);
    } catch(e) {
        debug("Failed to get with a null key");
    }

    try {
        index.getKey(null);
    } catch(e) {
        debug("Failed to getKey with a null key");
    }

    try {
        index.count(null);
    } catch(e) {
        debug("Failed to count with a null range");
    }
    
    objectStore.deleteIndex("TestIndex");
    
    try {
        index.get("test");
    } catch(e) {
        debug("Failed to get with deleted IDBIndex");
    }
    
    try {
        index.getKey("test");
    } catch(e) {
        debug("Failed to getKey with deleted IDBIndex");
    }
    
    try {
        index.count();
    } catch(e) {
        debug("Failed to count with deleted IDBIndex");
    }
    
    try {
        objectStore.deleteIndex("TestIndex");
    } catch(e) {
        debug("Failed to delete a nonexistent IDBIndex");
    }

    database.deleteObjectStore("TestObjectStore");
    try {
        index.get(null);
    } catch(e) {
         debug("Failed to get with deleted IDBObjectStore");
    }

    try {
        index.getKey(null);
    } catch(e) {
         debug("Failed to getKey with deleted IDBObjectStore");
    }

    try {
        index.count(null);
    } catch(e) {
         debug("Failed to count with deleted IDBObjectStore");
    }

    var objectStore = database.createObjectStore("TestObjectStore2");
    objectStore.createIndex("TestIndex", "foo");
        
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
    var transaction = database.transaction("TestObjectStore2", "readonly");
    var objectStore = transaction.objectStore("TestObjectStore2");
    index = objectStore.index("TestIndex");

    // Spin the transaction with get requests to keep it alive long enough for the setTimeout to fire.
    var canFinish = false;
    var spinGet = function() { 
        objectStore.get("foo").onsuccess = function() {
            if (!canFinish)
                spinGet();
        }
    }
    spinGet();

    var testWhileInactive = function() {
        try {
            index.get(null);
        } catch(e) {
             debug("Failed to get while transaction inactive");
        }

        try {
            index.getKey(null);
        } catch(e) {
             debug("Failed to getKey while transaction inactive");
        }

        try {
            index.count(null);
        } catch(e) {
             debug("Failed to count while transaction inactive");
        }
        canFinish = true;
    }
    
    setTimeout(testWhileInactive, 0);
    
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


