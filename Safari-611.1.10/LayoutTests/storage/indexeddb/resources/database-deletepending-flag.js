if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB IDBDatabase internal delete pending flag");

indexedDBTest(prepareDatabase, testDatabaseDelete, {"version": 5});
function prepareDatabase()
{
    connection = event.target.result;
    evalAndLog("connection.createObjectStore('store')");
    shouldBe("connection.objectStoreNames.length", "1");
}

function testDatabaseDelete()
{
    debug("");
    debug("Issue a delete request against the database - should be blocked by the open connection:");
    evalAndLog("deleteRequest = indexedDB.deleteDatabase(dbname)");
    deleteRequest.onerror = unexpectedErrorCallback;
    evalAndLog("state = 0");

    debug("");
    debug("Open a second connection - should be delayed:");
    evalAndLog("openRequest = indexedDB.open(dbname)");
    openRequest.onerror = unexpectedErrorCallback;

    connection.onversionchange = function() {
        debug("");
        debug("connection received versionchange event - ignoring.");
        shouldBe("++state", "1");
    };

    deleteRequest.onblocked = function() {
        debug("");
        debug("deleteRequest received blocked event.");
        shouldBe("++state", "2");
        evalAndLog("connection.close()");
        debug("deleteRequest should now be unblocked.");
    };

    deleteRequest.onsuccess = function() {
        debug("");
        debug("deleteRequest received success event.");
        shouldBe("++state", "3");
        debug("openRequest should now be unblocked.");
    };

    openRequest.onsuccess = function() {
        debug("");
        testPassed("openRequest received success event.");
        shouldBe("++state", "4");
        evalAndLog("connection2 = openRequest.result");

        debug("connection2 should reference a different database:");
        shouldBeFalse("connection2.version == connection.version");
        shouldBe("connection2.objectStoreNames.length", "0");

        debug("");
        finishJSTest();
    };
}
