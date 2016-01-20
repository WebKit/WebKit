description("This test exercises the obvious ways that IDBDatabase.createObjectStore can fail.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

function dumpObjectStores(database) {
    var list = database.objectStoreNames;
    debug("Object store names:");
    for (var i = 0; i < list.length; ++i) { 
        debug(list[i]);
    }
}

var createRequest = window.indexedDB.open("CreateObjectStoreFailuresTestDatabase", 1);
var database;
var versionTransaction;

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    versionTransaction = createRequest.transaction;
    database = event.target.result;

    try {
        var objectStore = database.createObjectStore('TestObjectStore', { autoIncrement: true , keyPath: "" });
    } catch (e) {
        debug("Failed to create object store with both autoincrement and an empty keypath: " + e);
        dumpObjectStores(database);
    }

    try {
        var objectStore = database.createObjectStore('TestObjectStore', { autoIncrement: true , keyPath: ['foo'] });
    } catch (e) {
        debug("Failed to create object store with both autoincrement and a sequence keypath: " + e);
        dumpObjectStores(database);
    }
    
    try {
        var objectStore = database.createObjectStore('TestObjectStore', { keyPath: "'foo bar'"});
    } catch (e) {
        debug("Failed to create object store with invalid keyPath: " + e);
        dumpObjectStores(database);
    }

    database.createObjectStore("TestObjectStore1");
    debug("Actually created an object store");
    dumpObjectStores(database);
    
    try {
        database.createObjectStore("TestObjectStore1");
    } catch(e) {
        debug("Failed to create TestObjectStore a second time: " + e);
        dumpObjectStores(database);
    }

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange unexpected abort");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction complete");
        dumpObjectStores(database);

        try {
            database.createObjectStore("TestObjectStore2");
        } catch(e) {
            debug("Failed to create object store while there is no version change transaction: " + e);
            dumpObjectStores(database);
        }
    
        setTimeout(finishUp, 0);
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function finishUp()
{
    try {
        database.createObjectStore("TestObjectStore99");
    } catch(e) {
        debug("Failed to create object store outside of onupgradeneeded: " + e);
        dumpObjectStores(database);
    }
    
    done();
}
