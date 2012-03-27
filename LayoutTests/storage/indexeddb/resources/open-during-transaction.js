if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB opening database connections during transactions");

function test()
{
    removeVendorPrefixes();
    prepareDatabase();
}

function prepareDatabase()
{
    debug("prepare database");
    evalAndLog("openreq1 = indexedDB.open('db1')");
    openreq1.onerror = unexpectedErrorCallback;
    openreq1.onsuccess = function (e) {
        evalAndLog("dbc1 = openreq1.result");
        evalAndLog("setverreq = dbc1.setVersion('1.0')");
        setverreq.onerror = unexpectedErrorCallback;
        setverreq.onsuccess = function (e) {
            evalAndLog("dbc1.createObjectStore('storeName')");
            setverreq.result.oncomplete = function (e) {
                debug("database preparation complete");
                debug("");
                startTransaction();
            };
        };
    };
}

function startTransaction()
{
    debug("starting transaction");
    evalAndLog("state = 'starting'");
    evalAndLog("trans = dbc1.transaction('storeName', IDBTransaction.READ_WRITE)");
    evalAndLog("trans.objectStore('storeName').put('value', 'key')");
    trans.onabort = unexpectedAbortCallback;
    trans.onerror = unexpectedErrorCallback;
    trans.oncomplete = function (e) {
        debug("transaction complete");
        shouldBeEqualToString("state", "open3complete");
        finishJSTest();
    };

    debug("");
    tryOpens();
}

function tryOpens()
{
    debug("trying to open the same database");
    evalAndLog("openreq2 = indexedDB.open('db1')");
    openreq2.onerror = unexpectedErrorCallback;
    openreq2.onsuccess = function (e) {
        debug("openreq2.onsuccess");
        shouldBeEqualToString("state", "starting");
        evalAndLog("state = 'open2complete'");
        debug("");
    }
    debug("");

    debug("trying to open a different database");
    evalAndLog("openreq3 = indexedDB.open('db2')");
    openreq3.onerror = unexpectedErrorCallback;
    openreq3.onsuccess = function (e) {
        debug("openreq3.onsuccess");
        shouldBeEqualToString("state", "open2complete");
        evalAndLog("state = 'open3complete'");
        debug("");
    }
    debug("");
}

test();