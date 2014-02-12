if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that integer versions are encoded/decoded consistently.");

versions = [1,
            0x7f,
            0x80,
            0x80000000,
            9007199254740991]; // 2^53-1, maximum JavaScript integer.

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = openFirstTime;
}

function openFirstTime(evt) {
    preamble(evt);

    if (!versions.length) {
        finishJSTest();
        return;
    }

    evalAndLog("version = " + versions.shift());
    evalAndLog("upgradeNeededFired = false");
    evalAndLog("request = indexedDB.open(dbname, version)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = function() {
        evalAndLog("upgradeNeededFired = true");
        evalAndLog("db = request.result");
        shouldBe("db.version", "version");
    };
    request.onsuccess = function() {
        shouldBeTrue("upgradeNeededFired");
        evalAndLog("db = request.result");
        shouldBe("db.version", "version");
        evalAndLog("db.close()");
        openSecondTime();
    };
}

function openSecondTime() {
    preamble();
    evalAndLog("request = indexedDB.open(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onsuccess = function() {
        evalAndLog("db = request.result");
        shouldBe("db.version", "version");
        evalAndLog("db.close()");
        openFirstTime();
    };
}

test();
