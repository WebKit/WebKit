description("This test exercises the \"unique\" constraint of indexes.");
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

var createRequest = window.indexedDB.open("Index3Database", 1);
var objectStore;

function checkObjectStore()
{
    var req1 = objectStore.get(1);
    req1.onsuccess = function() {
        debug("Value of 1 is: " + req1.result);
    }
    var req2 = objectStore.get(2);
    req2.onsuccess = function() {
        debug("Value of 2 is: " + req2.result);
    }
    var req3 = objectStore.get(3);
    req3.onsuccess = function() {
        debug("Value of 3 is: " + req3.result);
    }
}

function checkIndex(index, name)
{
    var req = index.count();
    req.onsuccess = function() {
        debug("Count in index " + name + " is: " + req.result);
    }
}

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");
    var i1 = objectStore.createIndex("TestIndex1", "bar", { unique: true });
    var i2 = objectStore.createIndex("TestIndex2", "bar", { multiEntry: true, unique: true });

    var request1 = objectStore.put({ bar: "good", baz: "bad" }, 1);
    request1.onsuccess = function() {
        debug("First put success");
    }
    request1.onerror = function() {
        debug("First put unexpected failure");
        done();
    }
    
    checkObjectStore();
    checkIndex(i1, 1);
    checkIndex(i2, 2);
    
    var request2 = objectStore.put({ bar: "good", baz: "bad" }, 2);
    request2.onsuccess = function() {
        debug("Second put unexpected success");
        done();
    }
    request2.onerror = function(e) {
        debug("Second put failure");
        e.stopPropagation();
        e.preventDefault();
    }
    
    checkObjectStore();
    checkIndex(i1, 1);
    checkIndex(i2, 2);
    
    var request3 = objectStore.put({ bar: [ "gnarly", "great", "good" ]}, 3);
    request3.onsuccess = function() {
        debug("Third put unexpected success");
        done();
    }
    request3.onerror = function(e) {
        debug("Third put failure");
        e.stopPropagation();
        e.preventDefault();
    }
    
    checkObjectStore();
    checkIndex(i1, 1);
    checkIndex(i2, 2);
    
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


