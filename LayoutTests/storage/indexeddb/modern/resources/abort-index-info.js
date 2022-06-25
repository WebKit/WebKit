description("Explores the edge cases of what IDBIndex objects look like after a version change transaction that changed them aborts.");

indexedDBTest(prepareDatabase, versionChangeSuccessCallback);

function prepareDatabase()
{
    evalAndLog("connection1 = event.target.result;");
    evalAndLog("objectStore1 = connection1.createObjectStore('objectStore');");
    evalAndLog("index1_1 = objectStore1.createIndex('foo', 'foo');");

    debug("");
    shouldBe("connection1.version", "1");
    shouldBe("objectStore1.indexNames.length", "1");
    shouldBeEqualToString("index1_1.name", "foo");
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
    debug("");
    evalAndLog("connection2 = event.target.result;");
    evalAndLog("objectStore2 = secondRequest.transaction.objectStore('objectStore');");
    evalAndLog("index2_1 = objectStore2.index('foo');");

    debug("");
    shouldBe("connection2.version", "2");
    shouldBe("objectStore2.indexNames.length", "1");
    shouldBeEqualToString("index2_1.name", "foo");
    
    debug("");
    evalAndLog("objectStore2.deleteIndex('foo');");
    evalAndLog("new_index2_1 = objectStore2.createIndex('foo', 'foo');");
    evalAndLog("index2_2 = objectStore2.createIndex('bar', 'bar');");

    debug("");
    shouldBe("objectStore2.indexNames.length", "2");
    shouldBeEqualToString("new_index2_1.name", "foo");
    shouldBeEqualToString("index2_2.name", "bar");

    debug("");
    evalAndLog("secondRequest.transaction.abort();");
}

function checkState()
{
    debug("");
    shouldBe("connection1.version", "1");
    shouldBe("objectStore1.indexNames.length", "1");
    shouldBeEqualToString("index1_1.name", "foo");
    shouldBe("connection2.version", "1");
    shouldBe("objectStore2.indexNames.length", "1");
    shouldBeEqualToString("index2_1.name", "foo");
    shouldBeEqualToString("new_index2_1.name", "foo");
    shouldBeEqualToString("index2_2.name", "bar");

    finishJSTest();
}
