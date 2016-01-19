description("This tests the most basic operation of the IDBIndex methods get(), getKey(), and count().");
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

var createRequest = window.indexedDB.open("IndexGetCountBasicDatabase", 1);

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var index = objectStore.createIndex("TestIndex", "bar");

    objectStore.put({ bar: "good", baz: "bad" }, "foo");
    
    var request1 = index.get("good");
    request1.onsuccess = function() {
        debug("get result is: " + request1.result);
        for (n in request1.result)
            debug(n + " is " + request1.result[n]);
    }

    request2 = index.getKey("good");
    request2.onsuccess = function() {
        debug("getKey result is: " + request2.result);
    }
    
    var request3 = index.count();
    request3.onsuccess = function() {
        debug("count result is: " + request3.result);
    }
        
    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected aborted");
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


