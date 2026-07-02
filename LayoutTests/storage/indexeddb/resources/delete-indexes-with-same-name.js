if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Deleting different indexes with same name.");

function prepareDatabase()
{
    database = event.target.result;

    objectStore1 = database.createObjectStore('os1', { autoIncrement: true });
    objectStore1.createIndex('index', 'x');
    objectStore2 = database.createObjectStore('os2', { autoIncrement: true });
    objectStore2.createIndex('index', 'x');
}

function upgradeDatabase()
{
    database = event.target.result;
    version = database.version;
    database.close();

    openRequest = indexedDB.open(dbname, ++version);
    openRequest.onupgradeneeded = deleteIndexes;
    openRequest.onerror = unexpectedErrorCallback;
}

function deleteIndexes(event)
{
    database = event.target.result;
    transaction = event.target.transaction;
    transaction.onabort = finishJSTest;

    objectStore1 = transaction.objectStore('os1');
    evalAndLog("objectStore1.deleteIndex('index')");
    objectStore2 = transaction.objectStore('os2');
    evalAndLog("objectStore2.clear()");
    evalAndLog("objectStore2.deleteIndex('index')");

    // Perform an add operation to ensure delete requests are handled.
    request = objectStore1.add({ x:1 });
    request.onsuccess = () => {
        transaction.abort();
    };
    request.onerror = unexpectedErrorCallback;
}

indexedDBTest(prepareDatabase, upgradeDatabase);
