description("This tests indexes are left in appropriate states after aborted transactions.");
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

var createRequest = window.indexedDB.open("Index2Database", 1);

function checkKey(index, key)
{
    var request1 = index.get(key);
    var request2 = index.getKey(key);

    request1.onsuccess = function() {
        debug("get \"" + key + "\" result is: " + request1.result);
        for (n in request1.result)
            debug(n + " is " + request1.result[n]);
    }
    request2.onsuccess = function() {
        debug("getKey \"" + key + "\" result is: " + request2.result);
        for (n in request2.result)
            debug(n + " is " + request2.result[n]);
    } 
}

function checkIndex(index)
{
    checkKey(index, "good");
    checkKey(index, "bad");
    checkKey(index, "multiEntry");
    checkKey(index, "test");
    
    var request = index.count();
    request.onsuccess = function() {
        debug("count result is: " + request.result);
        debug("");
    }
}

var database;

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var index1 = objectStore.createIndex("TestIndex1", "bar");
    var index2 = objectStore.createIndex("TestIndex2", "baz");
    var index3 = objectStore.createIndex("TestIndex3", "bar", { multiEntry: true });

    objectStore.put({ bar: "good", baz: "bad" }, 1);
    objectStore.put({ bar: [ "multiEntry", "test" ]}, 2);
    
    checkIndex(index1);
    checkIndex(index2);
    checkIndex(index3);

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected abort");
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
    var index1 = objectStore.index("TestIndex1");
    var index2 = objectStore.index("TestIndex2");
    var index3 = objectStore.index("TestIndex3");
    
    objectStore.delete(1).onsuccess = function() {
       debug("Deleted key 1 from objectstore");
       debug("");
    }

    checkIndex(index1);
    checkIndex(index2);
    checkIndex(index3);

    objectStore.clear().onsuccess = function() {
       debug("Cleared objectstore");
       debug("");
    }
    
    checkIndex(index1);
    checkIndex(index2);
    checkIndex(index3);
    
    objectStore.get(0).onsuccess = function() {
        debug("All done. Moving on to final part");
        transaction.abort();
    }
    
    transaction.onabort = function(event) {
        debug("readwrite transaction aborted");
        continueTest2();
    }

    transaction.oncomplete = function(event) {
        debug("readwrite transaction unexpected complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readwrite transaction unexpected error");
        done();
    }
}

function continueTest2()
{   
    var transaction = database.transaction("TestObjectStore", "readonly");
    var objectStore = transaction.objectStore("TestObjectStore");
    var index1 = objectStore.index("TestIndex1");
    var index2 = objectStore.index("TestIndex2");
    var index3 = objectStore.index("TestIndex3");
    
    checkIndex(index1);
    checkIndex(index2);
    checkIndex(index3);
    
    transaction.onabort = function(event) {
        debug("readwrite transaction unexpected abort");
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readwrite transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readwrite transaction unexpected error");
        done();
    }
}


