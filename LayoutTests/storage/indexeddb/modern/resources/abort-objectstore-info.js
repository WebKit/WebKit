description("Explores the edge cases of what IDBObjectStore objects look like after a version change transaction that changed them aborts.");

indexedDBTest(prepareDatabase, versionChangeSuccessCallback);

function prepareDatabase()
{
    evalAndLog("connection1 = event.target.result;");
    evalAndLog("objectStore1_1 = connection1.createObjectStore('objectStore1');");
    evalAndLog("objectStore1_2 = connection1.createObjectStore('objectStore2');");
    evalAndLog("objectStore1_2.createIndex('index', 'foo');");
    evalAndLog("objectStore1_4 = connection1.createObjectStore('objectStore4');");
    evalAndLog("objectStore1_4.createIndex('index', 'foo');");

    debug("");
    shouldBe("connection1.version", "1");
    shouldBe("connection1.objectStoreNames.length", "3");
    shouldBe("objectStore1_1.indexNames.length", "0");
    shouldBe("objectStore1_2.indexNames.length", "1");
    shouldBe("objectStore1_4.indexNames.length", "1");
    debug("");
}

function versionChangeSuccessCallback()
{
    evalAndLog("connection1.close();");
    evalAndLog("secondRequest = indexedDB.open(dbname, 2);");
    evalAndLog("secondRequest.onupgradeneeded = secondUpgradeNeeded;");
    secondRequest.onsuccess = unexpectedSuccessCallback;
    secondRequest.onerror = function() {
        evalAndLog("connection2.close()");
        checkState();
    };
}

function secondUpgradeNeeded()
{
    evalAndLog("connection2 = event.target.result;");
    evalAndLog("objectStore2_1 = secondRequest.transaction.objectStore('objectStore1');");
    evalAndLog("objectStore2_2 = secondRequest.transaction.objectStore('objectStore2');");
    evalAndLog("objectStore2_3 = connection2.createObjectStore('objectStore3');");
    evalAndLog("objectStore2_4 = secondRequest.transaction.objectStore('objectStore4');");

    debug("");
    shouldBe("connection2.version", "2");
    shouldBe("connection2.objectStoreNames.length", "4");
    shouldBe("objectStore2_1.indexNames.length", "0");
    shouldBe("objectStore2_2.indexNames.length", "1");
    shouldBe("objectStore2_3.indexNames.length", "0");
    shouldBe("objectStore2_4.indexNames.length", "1");
    
    debug("");
    evalAndLog("objectStore2_1.createIndex('index', 'foo');");
    evalAndLog("objectStore2_2.deleteIndex('index');");
    evalAndLog("objectStore2_3.createIndex('index', 'foo');");
    evalAndLog("objectStore2_4.deleteIndex('index');");
    evalAndLog("connection2.deleteObjectStore('objectStore4');");
    evalAndLog("new_objectStore2_4 = connection2.createObjectStore('objectStore4');");
    evalAndLog("new_objectStore2_4.createIndex('index1', 'foo');");
    evalAndLog("new_objectStore2_4.createIndex('index2', 'bar');");

    debug("");
    shouldBe("connection2.version", "2");
    shouldBe("connection2.objectStoreNames.length", "4");
    shouldBe("objectStore2_1.indexNames.length", "1");
    shouldBe("objectStore2_2.indexNames.length", "0");
    shouldBe("objectStore2_3.indexNames.length", "1");
    shouldBe("new_objectStore2_4.indexNames.length", "2");

    debug("");
    evalAndLog("secondRequest.transaction.abort();");
}

function checkState()
{
    debug("");
    debug("checkState():");

    shouldBe("connection1.version", "1");
    shouldBe("connection1.objectStoreNames.length", "3");
    shouldBe("objectStore1_1.indexNames.length", "0");
    shouldBe("objectStore1_2.indexNames.length", "1");
    shouldBe("objectStore1_4.indexNames.length", "1");
    debug("");

    shouldBe("connection2.version", "1");
    shouldBe("connection2.objectStoreNames.length", "3");
    shouldBe("objectStore2_1.indexNames.length", "0");
    shouldBe("objectStore2_2.indexNames.length", "1");
    shouldBe("objectStore2_3.indexNames.length", "0");
    shouldBe("objectStore2_4.indexNames.length", "1");
    shouldBe("new_objectStore2_4.indexNames.length", "0");

    finishJSTest();
}
