description("This tests some obvious failures that can happen while opening cursors.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var database;
var objectStore;
var index;

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");
    index = objectStore.createIndex("TestIndex", "bar");
    var request = objectStore.put({ bar: "bar" }, "foo");

    request.onsuccess = function() {
        try {
            objectStore.openCursor(NaN);
        } catch(e) {
            debug("Failed to open object store cursor with invalid keypath");
        }
        
        try {
            objectStore.openCursor("foo", "invalid direction");
        } catch(e) {
            debug("Failed to open object store cursor with invalid direction");
        }
        
        try {
            index.openCursor(NaN);
        } catch(e) {
            debug("Failed to open index cursor with invalid keypath");
        }

        try {
            index.openCursor("foo", "invalid direction");
        } catch(e) {
            debug("Failed to open index cursor with invalid direction");
        }

        try {
            index.openKeyCursor(NaN);
        } catch(e) {
            debug("Failed to open index key cursor with invalid keypath");
        }

        try {
            index.openKeyCursor("foo", "invalid direction");
        } catch(e) {
            debug("Failed to open index key cursor with invalid direction");
        }
        
        database.deleteObjectStore("TestObjectStore");
        
        try {
            objectStore.openCursor();
        } catch(e) {
            debug("Failed to open object store cursor on deleted object store");
        }
        
        try {
            index.openCursor();
        } catch(e) {
            debug("Failed to open index cursor on deleted object store");
        }

        try {
            index.openKeyCursor();
        } catch(e) {
            debug("Failed to open index key cursor on deleted object store");
        }
        
        // Recreate the objectstore because we'll need it in phase 2.
        objectStore = database.createObjectStore("TestObjectStore");
        index = objectStore.createIndex("TestIndex", "bar");
        objectStore.put({ bar: "bar" }, "foo");
    }
    
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
    try {
        objectStore.openCursor();
    } catch(e) {
        debug("Failed to open object store cursor from completed transaction");
    }
    
    try {
        index.openCursor();
    } catch(e) {
        debug("Failed to open index cursor from completed transaction");
    }

    try {
        index.openKeyCursor();
    } catch(e) {
        debug("Failed to open index key cursor from completed transaction");
    }
        
    var transaction = database.transaction("TestObjectStore", "readonly");
    objectStore = transaction.objectStore("TestObjectStore");
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

    var getWhileInactive = function() {
        try {
            objectStore.openCursor();
        } catch(e) {
            debug("Failed to open object store cursor from inactive transaction");
        }
    
        try {
            index.openCursor();
        } catch(e) {
            debug("Failed to open index cursor from inactive transaction");
        }

        try {
            index.openKeyCursor();
        } catch(e) {
            debug("Failed to open index key cursor from inactive transaction");
        }
        canFinish = true;
    }
    
    setTimeout(getWhileInactive, 0);
    
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


