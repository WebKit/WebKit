description("This tests using Date objects as keys and values.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("DateBasicDatabase", 1);
var database;

var date1 = new Date("1955-11-05T00:00:00");
var date2 = new Date("1955-11-12T18:00:00");
var date3 = new Date("2015-10-21T16:00:00");
    
createRequest.onupgradeneeded = function(event) {
    debug("ALERT: " + "Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");

    objectStore.put("Flux capacitor", date1);
    objectStore.put("Fish under the sea", date2);
    objectStore.put("Hoverboards", date3);
    
    objectStore.put(date1, "a");
    objectStore.put(date2, "b");
    objectStore.put(date3, "c");
    
    versionTransaction.onabort = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction unexpected aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction complete");
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

var objectStore;

function testGet(key) {
    var request = objectStore.get(key);
    request.onsuccess = function()
    {
        debug("ALERT: " + "Success getting key '" + key + "' of type " + typeof(key) + ", result is '" + request.result + "' of type " + typeof(request.result));
        if (key instanceof Date)
            debug("ALERT: " + "Key is a Date object, btw");
        if (request.result instanceof Date)
            debug("ALERT: " + "Result is a Date object, btw");
    }
    request.onerror = function()
    {
        debug("ALERT: " + "Expected error getting key '" + key + "'");
    }
}

function continueTest1()
{
    var transaction = database.transaction("TestObjectStore", "readonly");
    objectStore = transaction.objectStore("TestObjectStore");
    
    testGet(date1);
    testGet(date2);
    testGet(date3);
    testGet("a");
    testGet("b");
    testGet("c");
    
    transaction.onabort = function(event) {
        debug("ALERT: " + "readonly transaction unexpected abort" + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("ALERT: " + "readonly transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("ALERT: " + "readonly transaction unexpected error" + event);
        done();
    }
}
