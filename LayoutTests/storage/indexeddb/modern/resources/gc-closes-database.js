description("This tests that when IDBDatabase objects are destroyed in garbage collection that their database handles are closed.");

function done()
{
    finishJSTest();
}

function log(msg)
{
    debug(msg);
}

var databaseName;


deleteRequest = indexedDB.deleteDatabase("gc-closes-database");
deleteRequest.onsuccess = function() {
    log("Database deleted");
    continueTest();
}

deleteRequest.onerror = function() {
    log("Error deleting database");
    done();
}

function continueTest()
{
    log("Creating the default database");
    var openRequest = indexedDB.open("gc-closes-database");

    openRequest.onupgradeneeded = function(e) {
        log("Database upgraded to version 1");
    }
    
    openRequest.onsuccess = function(e) {
        log("Database created");
        setTimeout(window.gc, 0);
    }

    openRequest.onerror = function() {
        log("Error opening database");
        done();
    }

    hugeUpgradeOpen = indexedDB.open("gc-closes-database", 928375298375);
    hugeUpgradeOpen.onsuccess = function(e) {
        log("Huge upgrade open success");
        done();
    }
    hugeUpgradeOpen.onerror = function() {
        log("Unexpected error");
        done();
    }

}
