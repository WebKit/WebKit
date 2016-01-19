description("This test exercises various uses of IDBObjectStore.delete()");


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

var createRequest = window.indexedDB.open("IDBObjectStoreDelete1Database", 1);
var database;
var objectStore;

var date1 = new Date("1999-12-28T23:00:00");
var date2 = new Date("1999-12-29T23:00:00");
var date3 = new Date("1999-12-30T23:00:00");
var date4 = new Date("1999-12-31T23:00:00");
var date5 = new Date("2000-01-01T00:00:00");
var date6 = new Date("2000-01-02T00:00:00");
var date7 = new Date("2000-01-03T00:00:00");
var date8 = new Date("2000-01-04T00:00:00");

createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");
    
    // Just for the heck of it.
    objectStore.delete("foo");
    objectStore.put("boo", "foo");
    objectStore.delete("foo");

    objectStore.put("boo", "foo");
    objectStore.put("bop", "fop");
    objectStore.put("boq", "foq");
    objectStore.put("bor", "for");
    objectStore.put("bos", "fos");
    
    for (var i = 0; i < 100; ++i)
        objectStore.put("number " + i, i);

    objectStore.put("date 1", date1);
    objectStore.put("date 2", date2);
    objectStore.put("date 3", date3);
    objectStore.put("date 4", date4);
    objectStore.put("date 5", date5);
    objectStore.put("date 6", date6);
    objectStore.put("date 7", date7);
    objectStore.put("date 8", date8);

    logCount();

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
    objectStore = transaction.objectStore("TestObjectStore");

    // The pairs of things in the array are the things to delete followed by the expected decrease in the total count of objects.
    var thingsToDelete = [
        date8, 1,
        date8, 0, 
        "balyhoo", 0, 
        IDBKeyRange.bound(date6, date7, true, true), 0,
        IDBKeyRange.bound(date6, date7, true), 1,
        IDBKeyRange.bound(date5, date6, false, true), 1,
        IDBKeyRange.bound(date1, date6), 5,
        IDBKeyRange.bound(1, 98), 98, 
        IDBKeyRange.bound("foo", "fos", true, true), 3
    ];
    var currentThing = 0;
    
    var runDeleteTests = function()
    {
        if (!thingsToDelete[currentThing])
            return;
        
        objectStore.delete(thingsToDelete[currentThing]).onsuccess = function() {
            debug("Deleted \"" + thingsToDelete[currentThing] + "\", and there should now be " + thingsToDelete[currentThing + 1] + " less things total.");
            logCount();
            currentThing += 2;
            runDeleteTests();
        }
    }
    runDeleteTests();
    
    transaction.onabort = function(event) {
        debug("readwrite transaction unexpected abort" + event);
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readwrite transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readwrite transaction unexpected error" + event);
        done();
    }
}


