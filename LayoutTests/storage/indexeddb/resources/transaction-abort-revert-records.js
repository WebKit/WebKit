if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test aborting transaction revert add record");

indexedDBTest(prepareDatabase, onDatabasePrepared);

function prepareDatabase()
{
    preamble(event);

    database = event.target.result;
    store = evalAndLog("store = database.createObjectStore('store')");
    evalAndLog("store.add('value1', 'key1')");
    evalAndLog("store.add('value2', 'key2')");
}

function onDatabasePrepared()
{
    preamble(event);

    evalAndLog("database = event.target.result");
    evalAndLog("database.close()");

    // Upgrade database and add records in versionchange transaction.
    openRequest = evalAndLog("openRequest = indexedDB.open(dbname, " + (database.version + 1) + ")");
    openRequest.onupgradeneeded = onDatabaseUpgrade;
    openRequest.onerror = onDatabaseError;
    openRequest.onsuccess = unexpectedSuccessCallback;
}

function onDatabaseUpgrade(event)
{
    preamble(event);

    request = event.target;
    transaction = evalAndLog("transcation = request.transaction");
    transaction.onabort = onTranactionAbort;
    evalAndLog("store = transcation.objectStore('store')");
    evalAndLog("store.delete('key2')");
    evalAndLog("store.add('value2_new', 'key2')");
    evalAndLog("store.add('value3', 'key3')");
    // Adding key again to trigger error that aborts transaction.
    evalAndLog("store.add('value3_new', 'key3')");
}

function onTranactionAbort(event)
{
    preamble(event);

    // Close database to unblock next open.
    evalAndLog("event.target.db.close()");
}

function onDatabaseError(event)
{
    preamble(event);

    // Open database again to check saved records.
    openRequest = evalAndLog("openRequest = indexedDB.open(dbname)");
    openRequest.onsuccess = onDatabaseOpen;
    openRequest.onerror = unexpectedErrorCallback;
}

function onDatabaseOpen(event)
{
    preamble(event);

    evalAndLog("database = event.target.result");
    shouldBe("database.version", "1");
    evalAndLog("transcation = database.transaction('store')");
    evalAndLog("store = transcation.objectStore('store')");
    request = evalAndLog("request = store.getAllKeys()");
    request.onsuccess = onGetAllKeysSuccess;
    request.onerror = unexpectedErrorCallback;
}

function onGetAllKeysSuccess(event)
{
    preamble(event);

    shouldBeEqualToString("JSON.stringify(event.target.result)", '["key1","key2"]');
    request = evalAndLog("request = store.get('key2')");
    request.onsuccess = onGetSuccess;
    request.onerror = unexpectedErrorCallback;
}

function onGetSuccess(event)
{
    preamble(event);

    shouldBeEqualToString("event.target.result", "value2");
    finishJSTest();
}
