description("This test various uses of advance() and continue() on a \"next\" cursor.");

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

function setupRequest1(request)
{
    request.onsuccess = function() {
        if (!request.result) {
            setupRequest2(objectStore.openCursor());
            return;
        }
        debug("Success iterating cursor");
        logCursor(request.result);
        request.result.continue(request.result.key + 2);
    }
    request.onerror = function(e) {
        debug("Error iterating cursor");
        done();
    } 
}

function setupRequest2(request)
{
    request.onsuccess = function() {
        if (!request.result)
            return;
        debug("Success iterating cursor");
        logCursor(request.result);
        request.result.advance(3);        
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
    
    setupRequest1(objectStore.openCursor());
      
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
