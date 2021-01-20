if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that integer versions are retained when backing store is opened/closed/re-opened.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    // NOTE: deleteDatabase is not used, otherwise it would set the backing store schema version
    // which would conceal the failure being tested for. Instead, a unique name is used each time.
    evalAndLog("dbname = dbname + Date.now()");

    openFirstTime();
}

function openFirstTime() {
    preamble();
    request = evalAndLog("indexedDB.open(dbname, 1)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    upgradeNeededFired = false;
    request.onupgradeneeded = function() {
        evalAndLog("db = request.result");
        evalAndLog("upgradeNeededFired = true");
        shouldBe("db.version", "1");
        shouldBe("db.objectStoreNames.length", "0");
        evalAndLog("db.createObjectStore('store')");
    };
    request.onsuccess = function() {
        shouldBeTrue("upgradeNeededFired");
        evalAndLog("db = request.result");
        shouldBe("db.version", "1");
        shouldBe("db.objectStoreNames.length", "1");
        evalAndLog("db.close()");
        openSecondTime();
    };
}

function openSecondTime() {
    preamble();
    request = evalAndLog("indexedDB.open(dbname, 1)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onsuccess = function() {
        evalAndLog("db = request.result");
        shouldBe("db.version", "1");
        shouldBe("db.objectStoreNames.length", "1");
        evalAndLog("db.close()");
        debug("");
        finishJSTest();
    };
}

test();
