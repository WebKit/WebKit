description("This tests basic IDBCursor functionality");

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
    debug("Cursor is: " + cursor);
    debug("Cursor direction is: " + cursor.direction);
    debug("Cursor source is: " + cursor.source + " (" + cursor.source.name + ")");    
    debug("Cursor key is: " + cursor.key);    
    debug("Cursor primary key is: " + cursor.primaryKey);    
}

function setupRequest(request)
{
    // FIXME: Right now (until https://bugs.webkit.org/show_bug.cgi?id=151196 is resolved) what should be successful cursor operations will actually always fail.
    request.onsuccess = function() {
        debug("Success opening cursor");
        logCursor(request.result);  
    }
    request.onerror = function(e) {
        debug("Error opening cursor (expected for now)");
        logCursor(request.result);
        e.stopPropagation();
    } 
}

var objectStore;
var index;

function testCursorDirection(direction)
{
    var range = IDBKeyRange.lowerBound(-Infinity);
    var request = objectStore.openCursor(range, direction);
    setupRequest(request);
    request = index.openCursor(range, direction);
    setupRequest(request);
    request = index.openKeyCursor(range, direction);
    setupRequest(request);
}

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");
    index = objectStore.createIndex("TestIndex1", "bar");

    for (var i = 0; i < 10; ++i) {
        objectStore.put("Record " + i, i);
    }
    objectStore.put({ bar: "Hello" }, "foo");

    testCursorDirection("next");
    testCursorDirection("nextunique");
    testCursorDirection("prev");
    testCursorDirection("prevunique");
      
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
