description("This test creates some object stores, putting some values in them, committing the transaction. \
In a new transaction, it then overwrites those values, but then aborts the transaction. \
Finally it verifies everything is set up from the first transaction, and nothing from the aborted one committed.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

var dbname;

function prepareDatabase(event)
{
    debug("First upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
    event.target.onerror = null;
    var tx = event.target.transaction;
    var db = event.target.result;
    dbname = db.name;

    debug(tx + " - " + tx.mode);
    debug(db);

    var os1 = db.createObjectStore("TestObjectStore1");
    var os2 = db.createObjectStore("TestObjectStore2");

    var putRequest = os1.put("bar", "foo");
    
    putRequest.onsuccess = function(event) {
        debug("put succeeded - key was '" + putRequest.result + "'");
        
        var getRequest1 = os1.get("foo");
        getRequest1.onsuccess = function(event) {
            debug("get 'foo' succeeded - value was '" + getRequest1.result + "'");
        }

        getRequest1.onerror = function(event) {
            debug("get 'foo' unexpectedly failed - " + event);
            done();
        }
        
        var getRequest2 = os2.get("far");
        getRequest2.onsuccess = function(event) {
            debug("get 'far' succeeded - value was '" + getRequest2.result + "'");
        }

        getRequest2.onerror = function(event) {
            debug("get 'far' unexpectedly failed - " + event);
            done();
        }
    }

    putRequest.onerror = function(event) {
        debug("put unexpectedly failed - " + event);
        done();
    }
    
    tx.onabort = function(event) {
        debug("First version change transaction unexpected abort");
        done();
    }

    tx.oncomplete = function(event) {
        debug("First version change transaction completed");
        db.close();
        continueTest1();
    }

    tx.onerror = function(event) {
        debug("First version change transaction unexpected error - " + event);
        done();
    }
}

function continueTest1()
{
    var request = window.indexedDB.open(dbname, 2);

    request.onupgradeneeded = function(event) {
        debug("Second upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
        var tx = request.transaction;
        var db = event.target.result;

        debug(tx + " - " + tx.mode);
        debug(db);

        var os1 = tx.objectStore("TestObjectStore1");
        var os2 = tx.objectStore("TestObjectStore2");
        var putRequest1 = os1.put("baz", "foo");
        var putRequest2 = os2.put("boo", "far");
    
        putRequest1.onerror = function(e) {
            debug("Unexpected error overwriting bar with baz - " + e);
            done();
        }

        putRequest2.onerror = function(e) {
            debug("Writing into second object store unexpectedly failed - " + e);
            done();
        }
        
        putRequest2.onsuccess = function(e) {
            var getRequest1 = os1.get("foo");
            var getRequest2 = os2.get("far");
            
            getRequest1.onsuccess = function(event) {
                debug("get1 'foo' succeeded - value was '" + getRequest1.result + "'");
            }

            getRequest1.onerror = function(event) {
                debug("get1 unexpectedly failed - " + event);
                done();
            }

            getRequest2.onsuccess = function(event) {
                debug("get2 'far' succeeded - value was '" + getRequest2.result + "'");
                tx.abort();
            }

            getRequest2.onerror = function(event) {
                debug("get2 unexpectedly failed - " + event);
                done();
            }
        }
        
        tx.onabort = function(event) {
            debug("Second version change transaction abort");
            db.close();
            continueTest2();
        }

        tx.oncomplete = function(event) {
            debug("Second version change transaction unexpected complete");
            done();
        }

        tx.onerror = function(event) {
            debug("Second version change transaction error - " + event);
        }
    }
}

function continueTest2()
{
    var request = window.indexedDB.open(dbname, 2);

    request.onupgradeneeded = function(event) {
        debug("Third upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
    
        var tx = request.transaction;
        var db = event.target.result;

        debug(tx + " - " + tx.mode);
        debug(db);

        var os1 = tx.objectStore("TestObjectStore1");
        var os2 = tx.objectStore("TestObjectStore2");
        var getRequest1 = os1.get("foo");
        var getRequest2 = os2.get("far");
    
        getRequest1.onsuccess = function(event) {
            debug("get1 'foo' succeeded - value was '" + getRequest1.result + "'");
        }

        getRequest1.onerror = function(event) {
            debug("get1 'foo' unexpectedly failed - " + event);
            done();
        }

        getRequest2.onsuccess = function(event) {
            debug("get2 'far' succeeded - value was '" + getRequest2.result + "'");
        }

        getRequest2.onerror = function(event) {
            debug("get2 'far' unexpectedly failed - " + event);
            done();
        }
        
        tx.onabort = function(event) {
            debug("Third version change transaction unexpected abort");
            done();
        }

        tx.oncomplete = function(event) {
            debug("Third version change transaction complete");
            done();
        }

        tx.onerror = function(event) {
            debug("Third version change transaction unexpected error - " + event);
            done();
        }
    }
}
