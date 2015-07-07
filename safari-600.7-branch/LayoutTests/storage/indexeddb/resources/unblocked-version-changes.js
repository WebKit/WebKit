if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure that metadata remains correct when an aborted version change is followed by another. ");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = openConnection;
}

function openConnection()
{
    preamble();
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = unexpectedSuccessCallback;
    request.onupgradeneeded = onUpgradeNeeded;
    request.onerror = onError;
}

function onUpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = request.result");
    shouldBe("db.version", "2");
    evalAndLog("transaction = request.transaction");

    evalAndLog("request = indexedDB.open(dbname, 3)");
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = onUpgradeNeeded2;
    request.onsuccess = onSuccess;

    evalAndLog("transaction.abort()");
}

function onError(evt)
{
    preamble(evt);
    shouldBe("db.version", "0");
}

function onUpgradeNeeded2(evt)
{
    preamble(evt);
    evalAndLog("db = request.result");
    shouldBe("db.version", "3");
}

function onSuccess(evt)
{
    preamble(evt);
    evalAndLog("db = request.result");
    shouldBe("db.version", "3");
    finishJSTest();
}

test();
