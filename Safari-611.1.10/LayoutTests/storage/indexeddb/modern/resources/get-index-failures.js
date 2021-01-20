description("This tests some obvious failures that can happen while calling IDBObjectStore.index().");

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
var dbname;
function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    dbname = database.name;
    var objectStore = database.createObjectStore("TestObjectStore");
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
    var transaction = database.transaction("TestObjectStore", "readonly");
    var objectStore = transaction.objectStore("TestObjectStore");

    var index = objectStore.index("TestIndex");
    debug("Got an index as expected: " + index);

    try {
        objectStore.index(null);
    } catch(e) {
        debug("Failed to get an index with a null name");
    }

    try {
        objectStore.index("DoesNotExistdex");
    } catch(e) {
        debug("Failed to get an index that doesn't exist");
    }

    transaction.onabort = function(event) {
        debug("readonly transaction unexpected abort" + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readonly transaction complete");
        continueTest2();
        database.close();
    }

    transaction.onerror = function(event) {
        debug("readonly transaction unexpected error" + event);
        done();
    }
}

function continueTest2()
{
    var createRequest = window.indexedDB.open(dbname, 2);
    createRequest.onupgradeneeded = function(event) {
        debug("Second upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        var versionTransaction = event.target.transaction;
        var database = event.target.result;
        var objectStore = versionTransaction.objectStore("TestObjectStore");
        database.deleteObjectStore("TestObjectStore");
        
        try {
            objectStore.index("TestIndex");
        } catch(e) {
            debug("Failed to get an index from a deleted object store");
        }        
            
        versionTransaction.onabort = function(event) {
            debug("Second versionchange transaction unexpected aborted");
            done();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Second versionchange transaction complete");
            done();
        }

        versionTransaction.onerror = function(event) {
            debug("Second versionchange transaction unexpected error" + event);
            done();
        }
    }
}
