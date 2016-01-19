description("This test exercises the readonly attributes on an IDBObjectStore.");
if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

var request = window.indexedDB.open("ObjectStoreAttributesTestDatabase");

function log(message)
{
    debug(message);
}

function done()
{
    finishJSTest();
}

var database;

request.onupgradeneeded = function(event)
{
    debug("First upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
    var tx = request.transaction;
    database = event.target.result;

    debug(tx + " - " + tx.mode);
    debug(database);

    var os1 = database.createObjectStore('TestObjectStore1', { autoIncrement: true , keyPath: "foo" });
    var os2 = database.createObjectStore('TestObjectStore2', { autoIncrement: false });

    debug(os1.name);
    debug(os2.name);
    debug(os1.autoIncrement);
    debug(os2.autoIncrement);
    debug(os1.keyPath);
    debug(os2.keyPath);
    debug(os1.transaction);
    debug(os2.transaction);
    debug(os1.transaction == tx);
    debug(os2.transaction == tx);
    debug(os1.transaction == os2.transaction);
    
    os2.createIndex("Foo index", "foo");
    os2.createIndex("Bar index", "bar");
    
    var names = os2.indexNames;
    debug("Object store has indexes:")
    for (var i = 0; i < names.length; ++i)
        debug(names[i]);
    
    os2.createIndex("Baz index", "baz");
    debug("After adding another, object store now has indexes:");
    names = os2.indexNames;
    for (var i = 0; i < names.length; ++i)
        debug(names[i]);
    
    tx.onabort = function(event) {
        debug("First version change transaction unexpected abort");
        done();
    }

    tx.oncomplete = function(event) {
        debug("First version change transaction completed");
        continueTest();
    }

    tx.onerror = function(event) {
        debug("First version change transaction unexpected error - " + event);
        done();
    }
}

function continueTest()
{
    var transaction = database.transaction("TestObjectStore2", "readonly");
    var objectStore = transaction.objectStore("TestObjectStore2");
    
    debug("In a new transaction, object store has indexes:");
    var names = objectStore.indexNames;
    for (var i = 0; i < names.length; ++i)
        debug(names[i]);

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


