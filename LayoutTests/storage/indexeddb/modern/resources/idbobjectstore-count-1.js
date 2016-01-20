description("This test exercises various uses of IDBObjectStore.count()");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("IDBObjectStoreCount1Database", 1);
var database;
var objectStore;

function getCount(arg)
{
    var request;
    if (arg == undefined)
        request = objectStore.count();
    else
        request = objectStore.count(arg);
    
    request.onsuccess = function() {
        debug("Count is: " + request.result);
    }
    request.onerror = function(error) {
        debug("Unexpected error getting count: " + error);
        done();
    }
}

function getCounts()
{
    getCount();
    getCount(IDBKeyRange.bound(3, 6));    
    getCount(IDBKeyRange.bound(3, 6, true, false));    
    getCount(IDBKeyRange.bound(3, 6, false, true));
    getCount(7);    
}

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");
    
    objectStore.put(1, 1);
    getCounts();
    objectStore.put(2, 2);
    getCounts();
    objectStore.put(3, 3);
    getCounts();
    objectStore.put(4, 4);
    getCounts();
    objectStore.put(5, 5);
    getCounts();
    objectStore.put(6, 6);
    getCounts();
    objectStore.put(7, 7);
    getCounts();
    objectStore.put(8, 8);
    getCounts();
    objectStore.put(9, 9);
    getCounts();
    objectStore.put(10, 10);
    getCounts();

    // FIXME: Once objectStore.delete() is implemented, also test counts after deleting previous records.
    
    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected abort");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        done();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

