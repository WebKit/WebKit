description("This test does basic testing of IDBObjectStore.add(), making sure that an attempt to overwrite an already-existing key fails with the appropriate error.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

var request = window.indexedDB.open("NewDatabaseAddTestDatabase");

function done()
{
    finishJSTest();
}

request.onupgradeneeded = function(event) {
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
    var tx = request.transaction;
    var db = event.target.result;

    debug(tx + " - " + tx.mode);
    debug(db);

    var os = db.createObjectStore("TestObjectStore");
    var putRequest1 = os.add("bar", "foo");
    var putRequest2 = os.add("baz", "foo");

    putRequest1.onsuccess = function(event) {
        debug("put 1 succeeded - key was '" + putRequest1.result + "'");
    }

    putRequest1.onerror = function(event) {
        debug("put 1 unexpectedly failed - " + event);
        done();
    }

    putRequest2.onsuccess = function(event) {
        debug("put 2 unexpectedly succeeded - key was '" + putRequest2.result + "'");
        done();
    }

    putRequest2.onerror = function(event) {
        debug("put 2 failed - " + event.type);

        var getRequest = os.get("foo");
        
        getRequest.onsuccess = function(event) {
            debug("get succeeded - key was '" + getRequest.result + "'");
        }

        getRequest.onerror = function(event) {
            debug("get unexpectedly failed - " + event.type);
            done();
        }
        
        event.stopPropagation();
        event.preventDefault();
    }
        
    tx.onabort = function(event) {
        debug("version change transaction unexpected abort");
        done();
    }

    tx.oncomplete = function(event) {
        debug("version change transaction completed");
        done();
    }

    tx.onerror = function(event) {
        debug("version change transaction unexpected error - " + event);
        done();
    }
}
