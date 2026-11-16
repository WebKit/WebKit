description("This tests cursors that iterate over entire indexes.");

indexedDBTest(prepareDatabase);


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

function testCursorDirection(index, direction)
{
    var range = IDBKeyRange.lowerBound(-Infinity);
    var request = index.openCursor(range, direction);
    setupRequest(request);
}

var index1;
var index2;

var cursorCommands = [
    "testCursorDirection(index2, 'prevunique')",
    "testCursorDirection(index1, 'prevunique')",
    "testCursorDirection(index2, 'prev')",
    "testCursorDirection(index1, 'prev')",
    "testCursorDirection(index2, 'nextunique')",
    "testCursorDirection(index1, 'nextunique')",
    "testCursorDirection(index2, 'next')",
    "testCursorDirection(index1, 'next')",
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
    
function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    index1 = objectStore.createIndex("TestIndex1", "bar");
    index2 = objectStore.createIndex("TestIndex2", "baz");

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

    var req1 = index1.count();
    req1.onsuccess = function() {
        debug("TestIndex1 count is: " + req1.result);
    }

    var req2 = index2.count();
    req2.onsuccess = function() {
        debug("TestIndex2 count is: " + req2.result);
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


