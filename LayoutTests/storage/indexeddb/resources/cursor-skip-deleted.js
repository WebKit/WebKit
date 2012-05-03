if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's cursor skips deleted entries.");

var names = ['Alpha', 'Bravo', 'Charlie', 'Delta', 'Echo', 'Foxtrot', 'Golf',
             'Hotel', 'India', 'Juliet', 'Kilo', 'Lima', 'Mike', 'November',
             'Oscar', 'Papa', 'Quebec', 'Romeo', 'Sierra', 'Tango', 'Uniform',
             'Victor', 'Whiskey', 'X-ray', 'Yankee', 'Zulu'];

test();

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('cursor-skip-deleted')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    var db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = setVersionSuccess;
    request.onerror = unexpectedErrorCallback;
}

function setVersionSuccess()
{
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = basicCursorTest;

    deleteAllObjectStores(db);

    var objectStore = evalAndLog("objectStore = db.createObjectStore('store', {keyPath: 'id'})");
    evalAndLog("objectStore.createIndex('nameIndex', 'name')");
    resetObjectStore(function() {});
}

var silentErrorHandler = function() { event.preventDefault(); }

function resetObjectStore(callback)
{
    debug("\nresetObjectStore():");
    if (callback === undefined)
        callback = function () {};

    var objectStore = trans.objectStore('store');
    for (var i = 0; i < names.length; i++)
        objectStore.delete(i).onerror = silentErrorHandler;
    for (var i = 0; i < names.length; i++)
        objectStore.add({id: i, name: names[i]}).onerror = unexpectedErrorCallback;

    debug("");
    callback();
}

function contains(arr, obj)
{
    for (var i = 0; i < arr.length; i++) {
        if (arr[i] == obj)
            return true;
    }
    return false;
}

var cursor;
var deleted;
var seen;

function testCursor(deleteList, createCursorCommand, callback)
{
    debug("\ntestCursor():");
    deleted = [];
    seen = [];

    // Create the cursor.
    request = evalAndLog(createCursorCommand);

    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        if (event.target.result == null) {
            // Make sure we have seen every non-deleted item.
            for (var i = 0; i < names.length; i++) {
                if (contains(deleted, i))
                    continue;

                if (!contains(seen, i))
                    testFailed("Cursor did not see item with id: " + i);
            }

            // Make sure we used every rule in |deleteList|.
            for (var i = 0; i < deleteList.length; i++) {
                if (!contains(seen, deleteList[i].id))
                    testFailed("deleteList rule with id: " + deleteList[i].id + " was never used.");
            }

            debug("");
            callback();
            return;
        }

        cursor = event.target.result;
        debug(event.target.result.value.id + ": " + event.target.result.value.name);
        seen.push(event.target.result.value.id);

        // Make sure we don't see any deleted items.
        if (contains(deleted, event.target.result.value.id))
            testFailed("Cursor hit previously deleted element.");

        for (var i = 0; i < deleteList.length; i++) {
            if (event.target.result.value.id == deleteList[i].id) {
                // Delete objects targeted by this id.
                var targets = deleteList[i].targets;
                for (var j = 0; j < targets.length; j++) {
                    deleted.push(targets[j]);
                    request = evalAndLog("request = trans.objectStore('store').delete(" + targets[j] + ")");
                    request.onerror = unexpectedErrorCallback;
                    if (j == targets.length - 1)
                        request.onsuccess = function() { cursor.continue(); }
                }
                return;
            }
        }

        cursor.continue();
    }
}

function basicCursorTest()
{
    debug("basicCursorTest()");

    evalAndLog("trans = db.transaction(['store'], 'readwrite')");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = transactionComplete;

    var deletes = [{id: 1, targets: [0]},
                   {id: 2, targets: [names.length - 1]},
                   {id: 3, targets: [5,6,7]},
                   {id: 10, targets: [10]},
                   {id: 12, targets: [13]},
                   {id: 15, targets: [14]},
                   {id: 20, targets: [17,18]}
                   ];

    testCursor(deletes, "trans.objectStore('store').openCursor(IDBKeyRange.lowerBound(0))", function() { resetObjectStore(reverseCursorTest); });
}

function reverseCursorTest()
{
    debug("reverseCursorTest():");

    var deletes = [{id: 24, targets: [names.length - 1]},
                   {id: 23, targets: [0]},
                   {id: 22, targets: [20, 19, 18]},
                   {id: 15, targets: [15]},
                   {id: 13, targets: [12]},
                   {id: 10, targets: [11]},
                   {id: 5, targets: [7,8]}
                   ];

    testCursor(deletes, "trans.objectStore('store').openCursor(IDBKeyRange.lowerBound(0), 'prev')", function() { resetObjectStore(indexCursorTest); });
}

function indexCursorTest()
{
    debug("indexCursorTest():");

    var deletes = [{id: 1, targets: [0]},
                   {id: 2, targets: [names.length - 1]},
                   {id: 3, targets: [5,6,7]},
                   {id: 10, targets: [10]},
                   {id: 12, targets: [13]},
                   {id: 15, targets: [14]},
                   {id: 20, targets: [17,18]}
                   ];

    testCursor(deletes, "trans.objectStore('store').index('nameIndex').openCursor(IDBKeyRange.lowerBound('Alpha'))", function() { });
}

function transactionComplete()
{
    debug("transactionComplete():");
    finishJSTest();
}