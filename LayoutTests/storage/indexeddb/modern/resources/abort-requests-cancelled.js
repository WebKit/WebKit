description("This test makes sure that un-handled requests in a transaction receive onerror callbacks when the transaction is aborted.");

if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

function log(message)
{
    debug(message);
}

var database;
var objectStore;

function setupRequest(code)
{
    var request = eval(code);
    request.onsuccess = function()
    {
        log ("Success handling: \"" + code + "\"");
    }
    request.onerror = function(e)
    {
        log ("Error handling: \"" + code + "\" (" + e.type + ")");
        e.stopPropagation();
    }   
}

startTest();

function startTest() {
    var createRequest = window.indexedDB.open("AbortRequestsCancelledDatabase", 1);
    createRequest.onupgradeneeded = function(event) {
        debug("Initial upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        var versionTransaction = createRequest.transaction;
        var database = event.target.result;
        objectStore = database.createObjectStore("TestObjectStore");
        setupRequest("objectStore.put({ bar: 'A' }, 1);");
        setupRequest("objectStore.put({ bar: 'B' }, 2);");
        setupRequest("objectStore.put({ bar: 'C' }, 3);");

        versionTransaction.abort();

        versionTransaction.onabort = function(event) {
            debug("Initial upgrade versionchange transaction aborted");
            database.close();
            continueTest1();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Initial upgrade versionchange transaction unexpected complete");
            done();
        }

        versionTransaction.onerror = function(event) {
            debug("Initial upgrade versionchange transaction error " + event);
        }
    }
}

function continueTest1() {
    var createRequest = window.indexedDB.open("AbortRequestsCancelledDatabase", 1);
    createRequest.onupgradeneeded = function(event) {
        debug("Second upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

        var versionTransaction = createRequest.transaction;
        database = event.target.result;
        objectStore = database.createObjectStore("TestObjectStore");
    
        setupRequest("objectStore.put({ bar: 'A' }, 1);");
        setupRequest("objectStore.put({ bar: 'B' }, 2);");
        setupRequest("objectStore.put({ bar: 'C' }, 3);");
    
        versionTransaction.onabort = function(event) {
            debug("Second upgrade versionchange transaction unexpected abort");
            done();
        }

        versionTransaction.oncomplete = function(event) {
            debug("Second upgrade versionchange transaction completed");
            continueTest2();
        }

        versionTransaction.onerror = function(event) {
            debug("Second upgrade versionchange transaction unexpected error" + event);
            done();
        }
    }
}

function continueTest2() {
    var transaction = database.transaction("TestObjectStore", "readwrite");
    objectStore = transaction.objectStore("TestObjectStore");

    setupRequest("objectStore.get(1);");
    setupRequest("objectStore.get(2);");
    setupRequest("objectStore.get(3);");
    setupRequest("objectStore.put({ bar: 'D' }, 4);");
    setupRequest("objectStore.put({ bar: 'E' }, 5);");
    setupRequest("objectStore.put({ bar: 'F' }, 6);");

    transaction.abort();

    transaction.onabort = function(event) {
        debug("readwrite transaction aborted");
        continueTest3();
    }

    transaction.oncomplete = function(event) {
        debug("readwrite transaction unexpected complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readwrite transaction unexpected error" + event);
        done();
    }
}

function continueTest3() {
    var transaction = database.transaction("TestObjectStore", "readonly");
    objectStore = transaction.objectStore("TestObjectStore");

    setupRequest("objectStore.get(1);");
    setupRequest("objectStore.get(2);");
    setupRequest("objectStore.get(3);");

    transaction.abort();

    transaction.onabort = function(event) {
        debug("readonly transaction aborted");
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readwrite transaction unexpected complete");
        done();
    }

    transaction.onerror = function(event) {
        debug("readwrite transaction unexpected error" + event);
        done();
    }
}
