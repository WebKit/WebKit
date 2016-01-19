
if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

var request = window.indexedDB.open("NewDatabasePutTestDatabase");

function done()
{
    finishJSTest();
}

request.onupgradeneeded = function(event) {
    debug("ALERT: " + "Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
    var tx = request.transaction;
    var db = event.target.result;

    debug("ALERT: " + tx + " - " + tx.mode);
    debug("ALERT: " + db);

    var os = db.createObjectStore("TestObjectStore");
    var putRequest = os.put("bar", "foo");
    
    putRequest.onsuccess = function(event) {
        debug("ALERT: " + "put succeeded - key was '" + putRequest.result + "'");
        
        var getRequest = os.get("foo");
        getRequest.onsuccess = function(event) {
            debug("ALERT: " + "get succeeded - value was '" + getRequest.result + "'");
        }

        getRequest.onerror = function(event) {
            debug("ALERT: " + "get unexpectedly failed - " + event);
            done();
        }
    }

    putRequest.onerror = function(event) {
        debug("ALERT: " + "put unexpectedly failed - " + event);
        done();
    }
    
    tx.onabort = function(event) {
        debug("ALERT: " + "version change transaction unexpected abort");
        done();
    }

    tx.oncomplete = function(event) {
        debug("ALERT: " + "version change transaction completed");
        done();
    }

    tx.onerror = function(event) {
        debug("ALERT: " + "version change transaction unexpected error - " + event);
        done();
    }
}
