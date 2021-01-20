description("This tests that indexes added to an object store with existing records are populated upon their creation.");

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

    var cursorRequest = index.openCursor();
    cursorRequest.onsuccess = function() {
        var cursor = cursorRequest.result;
        if (!cursor) {
            done();
            return;
        }
        debug("Cursor at record: " + cursor.key + " / " + cursor.primaryKey);
        cursor.continue();

    }
    cursorRequest.onerror = function(e) {
        debug("Unexpected error opening or iterating cursor");
        logCursor(cursorRequest.result);
        done();
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

    // This index should be populated with the above values upon its creation.
    index = objectStore.createIndex("TestIndex1", "bar");
    
    checkIndexValues();
    
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


