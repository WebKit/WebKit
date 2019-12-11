description("This tests that index cursors properly handle changing indexes.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var index;
var objectStore;

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
        debug("Success opening or iterating cursor");
        if (request.result)
            logCursor(request.result);

        if (request.result && request.iteratedOnce) {
            var primaryKey = request.result.primaryKey;
            if (primaryKey) {
                objectStore.delete(primaryKey).onsuccess = function() {
                    debug("Deleted key " + primaryKey + " from object store");
                }
                var nextPrimaryKey = primaryKey;
                if (request.result.direction.startsWith("next")) {
                    nextPrimaryKey++;
                    if (nextPrimaryKey > 18)
                        nextPrimaryKey = 0;
                } else
                    nextPrimaryKey--;

                if (nextPrimaryKey > 0) {
                    objectStore.delete(nextPrimaryKey).onsuccess = function() {
                        debug("Deleted key " + nextPrimaryKey + " from object store");
                    }
                }
                
                // Delete an additional item for unique cursors to make sure they iterate deeper into the sets
                // of primary keys and/or skip some index keys altogether.
                if (request.result.direction.endsWith("unique")) {                
                    var nextNextPrimaryKey = nextPrimaryKey;
                    if (request.result.direction.startsWith("next")) {
                        nextNextPrimaryKey++;
                        if (nextNextPrimaryKey > 18)
                            nextNextPrimaryKey = 0;
                    } else
                        nextNextPrimaryKey--;

                    if (nextNextPrimaryKey > 0) {
                        objectStore.delete(nextNextPrimaryKey).onsuccess = function() {
                            debug("Deleted key " + nextNextPrimaryKey + " from object store");
                        }
                    }
                }
            }
        }
     
        request.iteratedOnce = true;

        if (request.result)
            request.result.continue();
        else
            startNextCursor();
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

var cursorCommands = [
    "testCursorDirection(index, 'prevunique')",
    "testCursorDirection(index, 'nextunique')",
    "testCursorDirection(index, 'prev')",
    "testCursorDirection(index, 'next')",
];

function startNextCursor()
{
    if (!cursorCommands.length) {
        done();
        return;
    }
    
    populateObjectStore();

    var command = cursorCommands.pop();
    debug("Starting a new cursor: " + command);
    var req = index.count();
    req.onsuccess = function() {
        debug("TestIndex1 count is: " + req.result + "");
    }
    
    eval(command);
}

function populateObjectStore()
{
    objectStore.put({ bar: "A" }, 1);
    objectStore.put({ bar: "A" }, 2);
    objectStore.put({ bar: "B" }, 3);
    objectStore.put({ bar: "B" }, 4);
    objectStore.put({ bar: "C" }, 5);
    objectStore.put({ bar: "C" }, 6);
    objectStore.put({ bar: "D" }, 7);
    objectStore.put({ bar: "D" }, 8);
    objectStore.put({ bar: "E" }, 9);
    objectStore.put({ bar: "E" }, 10);
    objectStore.put({ bar: "F" }, 11);
    objectStore.put({ bar: "F" }, 12);
    objectStore.put({ bar: "G" }, 13);
    objectStore.put({ bar: "G" }, 14);    
    objectStore.put({ bar: "H" }, 15);
    objectStore.put({ bar: "H" }, 16);  
    objectStore.put({ bar: "I" }, 17);
    objectStore.put({ bar: "I" }, 18);  
}

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");
    index = objectStore.createIndex("TestIndex1", "bar");
    
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


