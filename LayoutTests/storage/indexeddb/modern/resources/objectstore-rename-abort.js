description("This test verifies renaming object store can be reverted succesfully if version change transaction aborts.");

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
    evalAndLog("objectStore = database.createObjectStore('os1');");
    evalAndLog("database.deleteObjectStore('os1');");
    evalAndLog("objectStore2 = database.createObjectStore('os2')");
    evalAndLog("objectStore2.name = 'os1';");
    evalAndLog("objectStore2.put(1, 1).onsuccess = () => { transaction.abort(); }");
}

function onTransactionAbort()
{
    database.close();
    evalAndLog("newOpenRequest = indexedDB.open(dbname);");
    newOpenRequest.onupgradeneeded = (event) => {
        evalAndLog("newDatabase = newOpenRequest.result;");
        shouldBe("newDatabase.objectStoreNames.length", "0");
    }
    newOpenRequest.onerror = unexpectedErrorCallback;
    newOpenRequest.onsuccess = finishJSTest;
}

openDatabase();