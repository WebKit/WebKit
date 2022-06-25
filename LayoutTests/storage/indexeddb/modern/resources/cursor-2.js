description("This test checks basic functionality walking a \"next\" and \"prev\" cursor on an object store with some records.");

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
    debug("Cursor value is: " + cursor.value);    
    debug("Cursor primary key is: " + cursor.primaryKey);    
}

var objectStore;
var shouldStartPrevious = true;
function setupRequest(request)
{
    request.onsuccess = function() {
        if (!request.result) {
            if (shouldStartPrevious) {
                setupRequest(objectStore.openCursor(IDBKeyRange.lowerBound(-Infinity), "prev"));
                shouldStartPrevious = false;
            }
            return;
        }

        debug("Success iterating " + request.result.direction + " cursor");
        logCursor(request.result);
        request.result.continue();
    }

    request.onerror = function(e) {
        debug("Error iterating cursor");
        done();
    } 
}

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");

    for (var i = 0; i < 10; ++i) {
        objectStore.put("Record " + i, i);
    }
    objectStore.put({ bar: "Hello" }, "foo");
    
    setupRequest(objectStore.openCursor());
      
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
