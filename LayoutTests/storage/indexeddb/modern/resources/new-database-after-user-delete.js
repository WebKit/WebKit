description("Tests that if you perform a user delete with an open database connection, and then you re-open the same database, that the new database is actually new.");

indexedDBTest(prepareDatabase);

function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var dbName;
function prepareDatabase(event)
{
    log("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    dbName = database.name;
    
    var objectStore = database.createObjectStore("TestObjectStore");
    objectStore.put("bar", "foo").onsuccess = function() {
        log("Value was stored in objectStore");
    };
    
    log("Created an objectStore an put a value in it");
    
    database.onerror = function(event) {
        log("Database connection error'ed out. Opening a new connection.");
        continueTest();
    }

    versionTransaction.onabort = function(event) {
        log("Initial upgrade versionchange transaction unexpected abort: " + event.type + " " + versionTransaction.error.name + ", " + versionTransaction.error.message);
        done();
    }

    versionTransaction.oncomplete = function() {
        log("Version change transaction completed. Version is now " + database.version + ". About to request clearAllDatabases");
        if (window.testRunner) {
            setTimeout("testRunner.clearAllDatabases();", 0);
            log("Requested clearAllDatabases");
        }
    }

    versionTransaction.onerror = function(event) {
        log("Initial upgrade versionchange transaction unexpected error: " + event.type + " " + versionTransaction.error.name + ", " + versionTransaction.error.message);
        done();
    }
}

function continueTest()
{
    var openReq = indexedDB.open(dbName);
    
    openReq.onblocked = function() {
        log("Second open request unexpected blocked");
        done();
    }

    openReq.onupgradeneeded = function(event) {
        log("Reopened upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);
        if (event.target.result.objectStoreNames.length)
            log("[FAIL] The database has object stores already, but shouldn't");
        else
            log("[PASS] The database has no object stores, meaning it is new");
        done();
    }

    openReq.onsuccess = function() {
        log("Second open request unexpected success");
        done();
    }
}
