description("This tests that opening a cursor with a 'nextunique'/'prevunique' direction over a key range that matches no records succeeds with a null result, instead of firing an error. See rdar://182231936.");

indexedDBTest(prepareDatabase, openSuccess);

function done()
{
    finishJSTest();
}

var database;

// shouldBeNull() evaluates its argument in the global scope, so `request` must be a
// global for it to be visible there rather than scoped to the enclosing test function.
var request;

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var db = event.target.result;
    var objectStore = db.createObjectStore("TestObjectStore");
    objectStore.createIndex("TestIndex", "bar");

    objectStore.put({ bar: "A" }, 1);
    objectStore.put({ bar: "B" }, 2);
}

function openSuccess(event)
{
    database = event.target.result;
    runNextTest();
}

var tests = [
    testIndexCursor("nextunique"),
    testIndexCursor("prevunique"),
    testIndexCursor("next"),
    testObjectStoreCursor("nextunique"),
];

function runNextTest()
{
    if (!tests.length) {
        done();
        return;
    }

    tests.shift()();
}

// A key range that cannot match any record ("bar" values used are only "A" and "B").
function emptyIndexRange()
{
    return IDBKeyRange.bound("X", "Z");
}

// A key range that cannot match any record (object store keys used are only 1 and 2).
function emptyObjectStoreRange()
{
    return IDBKeyRange.lowerBound(1000);
}

function testIndexCursor(direction)
{
    return function() {
        debug("");
        debug("Opening index cursor with direction '" + direction + "' over an empty range");

        var transaction = database.transaction("TestObjectStore", "readonly");
        request = transaction.objectStore("TestObjectStore").index("TestIndex").openCursor(emptyIndexRange(), direction);
        request.onsuccess = function() {
            shouldBeNull("request.result");
            runNextTest();
        };
        request.onerror = function(event) {
            testFailed("Error function called unexpectedly: (" + event.target.error.name + ") " + event.target.error.message);
            runNextTest();
        };
    };
}

function testObjectStoreCursor(direction)
{
    return function() {
        debug("");
        debug("Opening object store cursor with direction '" + direction + "' over an empty range");

        var transaction = database.transaction("TestObjectStore", "readonly");
        request = transaction.objectStore("TestObjectStore").openCursor(emptyObjectStoreRange(), direction);
        request.onsuccess = function() {
            shouldBeNull("request.result");
            runNextTest();
        };
        request.onerror = function(event) {
            testFailed("Error function called unexpectedly: (" + event.target.error.name + ") " + event.target.error.message);
            runNextTest();
        };
    };
}
