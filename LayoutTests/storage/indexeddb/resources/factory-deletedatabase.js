if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's webkitIndexedDB.deleteDatabase().");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('database-to-delete')");
    request.onsuccess = startSetVersion;
    request.onerror = unexpectedErrorCallback;
}

function startSetVersion()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = deleteExisting;
    request.onerror = unexpectedErrorCallback;
}

function deleteExisting()
{
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");

    store = evalAndLog("store = db.createObjectStore('storeName', null)");

    self.index = evalAndLog("store.createIndex('indexName', '')");
    shouldBeTrue("store.indexNames.contains('indexName')");

    request = evalAndLog("store.add('value', 'key')");
    request.onerror = unexpectedErrorCallback;
    trans.oncomplete = getValue;
}

function getValue()
{
    transaction = evalAndLog("db.transaction('storeName', IDBTransaction.READ_WRITE)");
    transaction.onabort = unexpectedErrorCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    request = evalAndLog("store.get('key')");
    request.onsuccess = addIndex;
    request.onerror = unexpectedErrorCallback;
}

function addIndex()
{
    shouldBeEqualToString("event.target.result", "value");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = deleteDatabase;
    request.onerror = unexpectedErrorCallback;
}

function deleteDatabase()
{
    db.onversionchange = function() { db.close(); }
    request = evalAndLog("request = indexedDB.deleteDatabase('database-to-delete')");
    request.onsuccess = reopenDatabase;
    request.onerror = unexpectedErrorCallback;
}

function reopenDatabase()
{
    request = evalAndLog("indexedDB.open('database-to-delete')");
    request.onsuccess = startSetVersionAgain;
    request.onerror = unexpectedErrorCallback;
}

function startSetVersionAgain()
{
    rdb = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = verifyNotFound;
    request.onerror = unexpectedErrorCallback;
}

function verifyNotFound()
{
    shouldBe("db.objectStoreNames.length", "0");

    finishJSTest();
}

test();