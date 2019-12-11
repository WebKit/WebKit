description("This test starts some version change transactions, creates some object stores, and variably commits or aborts the version change transactions. \
At various stages it verifies the object stores in the database are as-expected.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

function dumpObjectStores(database) {
    var list = database.objectStoreNames;
    debug("Object store names:");
    for (var i = 0; i < list.length; ++i) { 
        debug(list[i]);
    }
}

var dbname;
function prepareDatabase(event)
{
    debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    event.target.onerror = null;

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    dbname = database.name;
    var objectStore = database.createObjectStore("FirstAbortedObjectStore");
    var request = objectStore.put("foo", "bar");

    request.onsuccess = function(event) {
        debug("Put succeeded");
        versionTransaction.abort();
    }
    request.onerror = function(event) {
        debug("Put failed - " + event);
        done();
    }
    
    dumpObjectStores(database);    
    
    versionTransaction.onabort = function(event) {
        debug("Initial upgrade versionchange transaction aborted");
        dumpObjectStores(database);
        continueTest1();
        database.close();
    }

    versionTransaction.oncomplete = function(event) {
        debug("Initial upgrade versionchange transaction unexpected complete");
        done();
    }

    versionTransaction.onerror = function(event) {
        debug("Initial upgrade versionchange transaction error " + event);
    }
}

function continueTest1()
{
    createRequest = window.indexedDB.open(dbname, 1);

    createRequest.onupgradeneeded = function(event) {
        debug("Second upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        var versionTransaction = event.target.transaction;
        var database = event.target.result;
        dumpObjectStores(database);
        var objectStore = database.createObjectStore("FirstCommittedObjectStore");

        versionTransaction.onabort = function(event) {
            debug("Second upgrade versionchange transaction unexpected abort");
            done();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Second upgrade versionchange transaction complete");
            dumpObjectStores(database);
            continueTest2();
            database.close();
        }

        versionTransaction.onerror = function(event) {
            debug("Second upgrade versionchange transaction unexpected error" + event);
            done();
        }
    }
}

function continueTest2()
{
    createRequest = window.indexedDB.open(dbname, 2);

    createRequest.onupgradeneeded = function(event) {
        debug("Third upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        var versionTransaction = event.target.transaction;
        var database = event.target.result;
        var objectStore = database.createObjectStore("SecondCommittedObjectStore");

        dumpObjectStores(database);    
    
        versionTransaction.onabort = function(event) {
            debug("Third upgrade versionchange transaction unexpected abort");
            done();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Third upgrade versionchange transaction complete");
            dumpObjectStores(database);
            database.close(); 
            continueTest3();
        }

        versionTransaction.onerror = function(event) {
            debug("Third upgrade versionchange transaction unexpected error" + event);
            done();
        }
    }
}

function continueTest3()
{
    createRequest = window.indexedDB.open(dbname, 3);

    createRequest.onupgradeneeded = function(event) {
        debug("Fourth upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
        var database = event.target.result;
        dumpObjectStores(database);
        done();
    }
}
