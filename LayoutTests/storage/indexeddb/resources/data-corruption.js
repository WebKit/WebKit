if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that data inserted into IndexedDB does not get corrupted on disk.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('data-corruption')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    self.db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = setVersionSuccess;
    request.onerror = unexpectedErrorCallback;
}

function setVersionSuccess()
{
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = addData;

    deleteAllObjectStores(db);

    evalAndLog("db.createObjectStore('storeName')");
}

var testDate = new Date('February 24, 1955 12:00:08');

function addData()
{
    debug("addData():");
    var transaction = evalAndLog("transaction = db.transaction(['storeName'], IDBTransaction.READ_WRITE)");
    var request = evalAndLog("request = transaction.objectStore('storeName').add({x: testDate}, 'key')");
    request.onerror = unexpectedErrorCallback;
    transaction.oncomplete = getData;
}

function getData()
{
    debug("addData():");
    var transaction = evalAndLog("transaction = db.transaction(['storeName'], IDBTransaction.READ_ONLY)");
    var request = evalAndLog("request = transaction.objectStore('storeName').get('key')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = doCheck;
}

function doCheck()
{
    shouldBeTrue("event.target.result.x.toString() == testDate.toString()");
    finishJSTest();
}

test();