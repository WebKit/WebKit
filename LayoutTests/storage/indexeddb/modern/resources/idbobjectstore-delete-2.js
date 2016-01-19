description("This test exercises IDBObjectStore.delete() followed by an abort to make sure the delete is un-done.");


if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function log(message)
{
    debug(message);
}

function logCount()
{
    var req = objectStore.count();
    req.onsuccess = function() { 
        debug("Count is " + req.result);
    }
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("IDBObjectStoreDelete2Database", 1);
var database;

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    database.createObjectStore("TestObjectStore").put("bar", "foo");

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
    transaction.objectStore("TestObjectStore").delete("foo").onsuccess = function()
    {
        var request = transaction.objectStore("TestObjectStore").get("foo");
        request.onsuccess = function()
        {
            debug("After delete, record for \"foo\" has value: " + request.result);
            transaction.abort();
        }
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

    var request = transaction.objectStore("TestObjectStore").get("foo");
    request.onsuccess = function()
    {
        debug("Record for \"foo\" has final value: " + request.result);
    }

    transaction.onabort = function(event)
    {
        debug("readonly transaction unexpected abort");
        done();
    }

    transaction.oncomplete = function(event)
    {
        debug("readonly transaction complete");
        done();
    }

    transaction.onerror = function(event)
    {
        debug("readonly transaction unexpected error");
        done();
    }
}


