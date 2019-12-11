description("This test exercises IDBObjectStore.get() with an IDBKeyRange as the parameter.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

var database;

var date1 = new Date("1955-11-05T00:00:00");
var date2 = new Date("1955-11-12T18:00:00");
var date3 = new Date("2015-10-21T16:00:00");
    
function prepareDatabase(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");

    objectStore.put("Flux capacitor", date1);
    objectStore.put("Fish under the sea", date2);
    objectStore.put("Hoverboards", date3);

    for (var i = 0; i < 100; ++i)
        objectStore.put("\"" + i + "\"", i);

    objectStore.put("PosInf", Infinity);
    objectStore.put("NegInf", -Infinity);

    objectStore.put("A", "A");
    objectStore.put("As", "As");
    objectStore.put("AS", "AS");
    objectStore.put("a", "a");

    objectStore.put("array 1", [1, "hello"]);
    objectStore.put("array 2", [2, "goodbye"]);
    objectStore.put("array 3", []);

    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction unexpected aborted");
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

var objectStore;

function testGet(keyRange) {
    var request = objectStore.get(keyRange);
    request.onsuccess = function()
    {
        debug("Success getting keyRange [" + keyRange.lower + " (" + (keyRange.lowerOpen ? "Open" : "Closed") + "), " + keyRange.upper + " (" + (keyRange.upperOpen ? "Open" : "Closed") + ")]");
        debug("Result is " + request.result);
    }
    request.onerror = function()
    {
        debug("Unexpected error getting keyRange [" + keyRange.lower + " (" + keyRange.lowerOpen + "), " + keyRange.upper + " (" + keyRange.upperOpen + ")]");
    }
}

function continueTest1()
{
    var transaction = database.transaction("TestObjectStore", "readonly");
    objectStore = transaction.objectStore("TestObjectStore");

    testGet(IDBKeyRange.lowerBound(-1));
    testGet(IDBKeyRange.lowerBound(-1, true));
    testGet(IDBKeyRange.lowerBound(0));
    testGet(IDBKeyRange.lowerBound(0, true));
    testGet(IDBKeyRange.lowerBound(0.1));
    testGet(IDBKeyRange.lowerBound(0.1, true));
    testGet(IDBKeyRange.lowerBound(99));
    testGet(IDBKeyRange.lowerBound(99, true));
    testGet(IDBKeyRange.lowerBound(99.1));
    testGet(IDBKeyRange.lowerBound(99.1, true));
    
    testGet(IDBKeyRange.upperBound(100));
    testGet(IDBKeyRange.upperBound(100, true));
    testGet(IDBKeyRange.upperBound(99));
    testGet(IDBKeyRange.upperBound(99, true));
    testGet(IDBKeyRange.upperBound(98.99999));
    testGet(IDBKeyRange.upperBound(98.99999, true));
    testGet(IDBKeyRange.upperBound(98));
    testGet(IDBKeyRange.upperBound(98, true));
    testGet(IDBKeyRange.upperBound(0));
    testGet(IDBKeyRange.upperBound(0, true));
    testGet(IDBKeyRange.upperBound(-0.1));
    testGet(IDBKeyRange.upperBound(-0.1, true));
    
    testGet(IDBKeyRange.bound(2.5, 3.5));
    testGet(IDBKeyRange.bound(-0.5, 0.5));
    testGet(IDBKeyRange.bound(98.5, 99.5));
    testGet(IDBKeyRange.bound(-1, 0));
    testGet(IDBKeyRange.bound(-1, 0, true));
    testGet(IDBKeyRange.bound(-1, 0, false, true));
    testGet(IDBKeyRange.bound(-1, 0, true, true));
    testGet(IDBKeyRange.bound(3, 4));
    testGet(IDBKeyRange.bound(3, 4, true));
    testGet(IDBKeyRange.bound(3, 4, false, true));
    testGet(IDBKeyRange.bound(3, 4, true, true));
    testGet(IDBKeyRange.bound(99, 100));
    testGet(IDBKeyRange.bound(99, 100, true));
    testGet(IDBKeyRange.bound(99, 100, false, true));
    testGet(IDBKeyRange.bound(99, 100, true, true));

    testGet(IDBKeyRange.bound(Infinity, "a"));
    testGet(IDBKeyRange.bound(Infinity, "a", true));
    testGet(IDBKeyRange.bound(Infinity, "a", false, true));
    testGet(IDBKeyRange.bound(Infinity, "a", true, true));

    testGet(IDBKeyRange.bound("AS", "a"));
    testGet(IDBKeyRange.bound("AS", "a", true));
    testGet(IDBKeyRange.bound("AS", "a", false, true));
    testGet(IDBKeyRange.bound("AS", "a", true, true));
    
    testGet(IDBKeyRange.bound(Infinity, []));
    testGet(IDBKeyRange.bound(Infinity, [], true));
    testGet(IDBKeyRange.bound(Infinity, [], false, true));
    testGet(IDBKeyRange.bound(Infinity, [], true, true));

    testGet(IDBKeyRange.bound(Infinity, "a"));
    testGet(IDBKeyRange.bound(Infinity, "a", true));
    testGet(IDBKeyRange.bound(Infinity, "a", false, true));
    testGet(IDBKeyRange.bound(Infinity, "a", true, true));

    testGet(IDBKeyRange.bound(Infinity, "a"));
    testGet(IDBKeyRange.bound(Infinity, "a", true));
    testGet(IDBKeyRange.bound(Infinity, "a", false, true));
    testGet(IDBKeyRange.bound(Infinity, "a", true, true));

    testGet(IDBKeyRange.bound(date1, date3));
    testGet(IDBKeyRange.bound(date1, date3, true));
    testGet(IDBKeyRange.bound(date1, date3, false, true));
    testGet(IDBKeyRange.bound(date1, date3, true, true));
    
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
