if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB IDBDatabase internal delete pending flag");

function test()
{
    removeVendorPrefixes();

    evalAndLog("dbname = self.location.pathname");
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = openConnection;
}

function openConnection()
{
    debug("");
    debug("Open a connection and set a sentinel version:");
    evalAndLog("version = '10'");
    evalAndLog("request = indexedDB.open(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("connection = request.result");
        evalAndLog("request = connection.setVersion(version)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function() {
            trans = request.result;
            trans.onabort = unexpectedAbortCallback;
            shouldBe("connection.version", "version");
            evalAndLog("connection.createObjectStore('store')");
            shouldBe("connection.objectStoreNames.length", "1");
            trans.oncomplete = testDatabaseDelete;
        };
    };
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

test();
