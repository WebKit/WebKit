description("This tests the expected values from some more complex index situations.");
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

var createRequest = window.indexedDB.open("Index1Database", 1);

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
    checkKey(index, "ok");
    checkKey(index, "meh");
    checkKey(index, "super");
    checkKey(index, "thanksForAsking");
    checkKey(index, [ "good", "bad" ]);
    checkKey(index, [ "ok", "meh" ]);
    checkKey(index, [ "super", "thanksForAsking" ]);
    checkKey(index, "This is to test");
    checkKey(index, "multiEntry indexes");
    checkKey(index, ["This is to test", "multiEntry indexes" ]);
    
    var request = index.count();
    request.onsuccess = function() {
        debug("count result is: " + request.result);
        debug("");
    }
}

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var index1 = objectStore.createIndex("TestIndex1", "bar");
    var index2 = objectStore.createIndex("TestIndex2", "baz");
    var index3 = objectStore.createIndex("TestIndex3", [ "bar", "baz" ]);
    var index4 = objectStore.createIndex("TestIndex4", "bar", { multiEntry: true });

    objectStore.put({ bar: "good", baz: "bad" }, 1);
    objectStore.put({ bar: "good", baz: "bad" }, 2);
    objectStore.put({ bar: "good", baz: "bad" }, 3);
    objectStore.put({ bar: "ok", baz: "meh" }, 4);
    objectStore.put({ bar: "ok", baz: "meh" }, 5);
    objectStore.put({ bar: "ok", baz: "meh" }, 6);
    objectStore.put({ bar: "super", baz: "thanksForAsking" }, 7);
    objectStore.put({ bar: "super", baz: "thanksForAsking" }, 8);
    objectStore.put({ bar: "super", baz: "thanksForAsking" }, 9);
    objectStore.put({ bar: [ "This is to test", "multiEntry indexes" ]}, 10);
    
    checkIndex(index1);
    checkIndex(index2);
    checkIndex(index3);
    checkIndex(index4);
  
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


