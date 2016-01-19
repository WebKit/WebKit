description("This tests cursors that iterate over parts of indexes.");
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

function logCursor(cursor)
{
    debug("Cursor direction is: " + cursor.direction);
    debug("Cursor source is: " + cursor.source.name);    
    debug("Cursor key is: " + cursor.key);    
    debug("Cursor primary key is: " + cursor.primaryKey);
    debug("Cursor value is: " + cursor.value);  
}

function setupRequest(request)
{
    request.onsuccess = function() {
        if (!request.result) {
            startNextCursor();
            return;
        }
        debug("Success opening or iterating cursor");
        logCursor(request.result);  
        request.result.continue();
    }
    request.onerror = function(e) {
        debug("Unexpected error opening or iterating cursor");
        logCursor(request.result);
        done();
    } 
}

var index;

function testCursorDirection(direction, range)
{
    var request = index.openCursor(range, direction);
    setupRequest(request);
}

var cursorCommands = [
    "testCursorDirection('prevunique', IDBKeyRange.only('B'))",
    "testCursorDirection('prev', IDBKeyRange.only('B'))",
    "testCursorDirection('nextunique', IDBKeyRange.only('B'))",
    "testCursorDirection('next', IDBKeyRange.only('B'))",
    "testCursorDirection('prevunique', IDBKeyRange.lowerBound('C'))",
    "testCursorDirection('prev', IDBKeyRange.lowerBound('C'))",
    "testCursorDirection('nextunique', IDBKeyRange.lowerBound('C'))",
    "testCursorDirection('next', IDBKeyRange.lowerBound('C'))",
    "testCursorDirection('prevunique', IDBKeyRange.upperBound('B'))",
    "testCursorDirection('prev', IDBKeyRange.upperBound('B'))",
    "testCursorDirection('nextunique', IDBKeyRange.upperBound('B'))",
    "testCursorDirection('next', IDBKeyRange.upperBound('B'))",
    "testCursorDirection('prevunique', IDBKeyRange.bound('B', 'C'))",
    "testCursorDirection('prev', IDBKeyRange.bound('B', 'C'))",
    "testCursorDirection('nextunique', IDBKeyRange.bound('B', 'C'))",
    "testCursorDirection('next', IDBKeyRange.bound('B', 'C'))",
    "testCursorDirection('prevunique', IDBKeyRange.bound('B', 'D', true, true))",
    "testCursorDirection('prev', IDBKeyRange.bound('B', 'D', true, true))",
    "testCursorDirection('nextunique', IDBKeyRange.bound('B', 'D', true, true))",
    "testCursorDirection('next', IDBKeyRange.bound('B', 'D', true, true))",
];

function startNextCursor()
{
    if (!cursorCommands.length) {
        done();
        return;
    }

    var command = cursorCommands.pop();
    log ("");
    debug("Starting a new cursor: " + command);
    eval(command);
}
    
var createRequest = window.indexedDB.open("IndexCursor2Database", 1);
createRequest.onupgradeneeded = function(event) {
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    index = objectStore.createIndex("TestIndex1", "bar");

    objectStore.put({ bar: "A", baz: "a" }, 1);
    objectStore.put({ bar: "A", baz: "a" }, 3);
    objectStore.put({ bar: "A", baz: "a" }, 2);
    objectStore.put({ bar: "B", baz: "b" }, 5);
    objectStore.put({ bar: "B", baz: "b" }, 6);
    objectStore.put({ bar: "B", baz: "b" }, 4);
    objectStore.put({ bar: "C", baz: "c" }, 7);
    objectStore.put({ bar: "C", baz: "c" }, 9);
    objectStore.put({ bar: "C", baz: "c" }, 8);
    objectStore.put({ bar: "D", baz: "d" }, 11);
    objectStore.put({ bar: "D", baz: "d" }, 12);
    objectStore.put({ bar: "D", baz: "d" }, 10);

    var req = index.count();
    req.onsuccess = function() {
        debug("TestIndex1 count is: " + req.result);
    }

    startNextCursor();
    
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


