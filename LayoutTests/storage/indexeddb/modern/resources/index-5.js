description("This tests creating an index on an object store that already has records, and those records would violate the unique constraint of the index. (The index creation should fail).");

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

function checkIndexValues()
{
    var countRequest = index.count();
    countRequest.onsuccess = function() {
        debug("Count is: " + countRequest.result);
    }
    countRequest.onerror = function(e) {
        debug("Error getting cursor count");
        e.stopPropagation();
    }

    var cursorRequest = index.openCursor();
    cursorRequest.onsuccess = function() {
        var cursor = cursorRequest.result;
        debug("Cursor at record: " + cursor.key + " / " + cursor.primaryKey);
        
        if (cursor.key != undefined)
            cursor.continue();
        else
            done();
    }
    cursorRequest.onerror = function(e) {
        debug("Error opening or iterating cursor");
        e.stopPropagation();
    } 
}

function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    objectStore = database.createObjectStore("TestObjectStore");
    objectStore.put({ bar: "A" }, 1);
    objectStore.put({ bar: "A" }, 2);

    // This index should be populated with the above values upon its creation, but that should fail because
    // of constraint violations.
    index = objectStore.createIndex("TestIndex1", "bar", { unique: true });
    
    checkIndexValues();
    
    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction aborted (expected because index creation failed, and that should've caused transaction abort)");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction unexpected complete");
        done();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}


