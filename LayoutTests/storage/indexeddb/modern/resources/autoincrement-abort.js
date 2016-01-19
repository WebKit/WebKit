description("This test creates a new database with an objectstore that autoincrements. \
It then puts some things in that object store, checking the keys that were used. \
But it then aborts that transaction. \
Then it opens a new one and puts something in it, double checking that the key generator was reverted when the above transaction was aborted.");



if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("AutoincrementAbortDatabase", 1);
var database;

createRequest.onupgradeneeded = function(event) {
    debug("ALERT: " + "Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore('TestObjectStore', { autoIncrement: true });
    
    versionTransaction.onabort = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction unexpected abort");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction complete");
        continueTest1();
    }

    versionTransaction.onerror = function(event) {
        debug("ALERT: " + "Initial upgrade versionchange transaction unexpected error" + event);
        done();
    }
}

function putChecker(event) {
    debug("ALERT: " + "Key used for put was " + event.target.result);
}

function continueTest1()
{
    debug("ALERT: " + "Opening readwrite transaction to bump the key generator, but it will be aborted");
    var transaction = database.transaction('TestObjectStore', "readwrite");
    var objectStore = transaction.objectStore('TestObjectStore');
    
    var request = objectStore.put("bar1");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar2");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar3");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar4");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar5");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar6");
    request.onsuccess = function(event) {
        putChecker(event);
        transaction.abort();
    }
    
    transaction.onabort = function(event) {
        debug("ALERT: " + "readwrite transaction abort");
        continueTest2();
    }

    transaction.oncomplete = function(event) {
        debug("ALERT: " + "readwrite transaction unexpected complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("ALERT: " + "readwrite transaction unexpected error");
        done();
    }
}

function continueTest2()
{
    debug("ALERT: " + "Opening readwrite transaction to make sure the key generator had successfully reverted");
    
    var transaction = database.transaction('TestObjectStore', "readwrite");
    var objectStore = transaction.objectStore('TestObjectStore');

    var request = objectStore.put("bar1");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar2");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar3");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar4");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar5");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar6");
    request.onsuccess = putChecker;

    transaction.onabort = function(event) {
        debug("ALERT: " + "readwrite transaction unexpected abort");
        done();
    }

    transaction.oncomplete = function(event) {
        debug("ALERT: " + "readwrite transaction complete");
        continueTest3();
    }

    transaction.onerror = function(event) {
        debug("ALERT: " + "readwrite transaction unexpected error");
        done();
    }
}

function continueTest3()
{
    debug("ALERT: " + "Opening readwrite transaction to make sure the key generator picks up where it should've left off");
    
    var transaction = database.transaction('TestObjectStore', "readwrite");
    var objectStore = transaction.objectStore('TestObjectStore');

    var request = objectStore.put("bar1");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar2");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar3");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar4");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar5");
    request.onsuccess = putChecker;
    var request = objectStore.put("bar6");
    request.onsuccess = putChecker;

    transaction.onabort = function(event) {
        debug("ALERT: " + "readwrite transaction unexpected abort");
        done();
    }

    transaction.oncomplete = function(event) {
        debug("ALERT: " + "readwrite transaction complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("ALERT: " + "readwrite transaction unexpected error");
        done();
    }
}


