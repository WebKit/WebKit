description("This test verifies version change transaction that adds and removes object store can be aborted succesfully.");

setDBNameFromPath();
function openDatabase()
{
    evalAndLog("openRequest = indexedDB.open(dbname);");
    openRequest.onupgradeneeded = onDatabaseUpgradeNeeded;
    openRequest.onsuccess = unexpectedSuccessCallback;
}

function onDatabaseUpgradeNeeded()
{
    evalAndLog("transaction = openRequest.transaction;");
    transaction.onabort = onTransactionAbort;
    evalAndLog("database = openRequest.result;");
    evalAndLog("database.createObjectStore('os1');");
    evalAndLog("database.deleteObjectStore('os1');");
    // Ensure delete request is proceeded.
    evalAndLog("objectStore = database.createObjectStore('os2')");
    objectStore.put(1, 1).onsuccess = () => { transaction.abort(); }
}

function onTransactionAbort()
{
    database.close();
    evalAndLog("newOpenRequest = indexedDB.open(dbname);");
    newOpenRequest.onupgradeneeded = (event) => {
        evalAndLog("newDatabase = newOpenRequest.result;");
        shouldBe("newDatabase.objectStoreNames.length", "0");
        evalAndLog("newObjectStore = newDatabase.createObjectStore('os1');");
        newObjectStore.put(1, 1).onsuccess = finishJSTest;
    }
    newOpenRequest.onerror = unexpectedErrorCallback;
    newOpenRequest.onsuccess = finishJSTest;
}

openDatabase();