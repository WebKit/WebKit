if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Check that a page reloaded during an in-flight upgradeneeded event doesn't hang.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();
    evalAndLog("dbname1 = dbname + '1'");
    evalAndLog("dbname2 = dbname + '2'");

    deleteDatabase1();
}

function deleteDatabase1()
{
    preamble();
    request = evalAndLog("indexedDB.deleteDatabase(dbname1)");
    request.onerror = unexpectedBlockedCallback;
    if (!isAfterReload())
        request.onblocked = unexpectedBlockedCallback;
    else
        debug("In a multi process implementation this deleteDatabase may be blocked temporarily, so we don't check for either the presence or absence of a blocked event.");
    request.onsuccess = deleteDatabase2;
}

function deleteDatabase2()
{
    preamble();
    request = evalAndLog("indexedDB.deleteDatabase(dbname2)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = openDatabase1;
}

function isAfterReload()
{
    return document.location.search == "?second";
}

function reload()
{
    document.location += "?second";
}

function openDatabase1()
{
    preamble();
    request = evalAndLog("indexedDB.open(dbname1, 1)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function openOnUpgradeNeeded1(evt) {
        preamble(evt);
        evalAndLog("db1 = event.target.result");
        evalAndLog("store1 = db1.createObjectStore('store')");
        evalAndLog("store1.put(0, 0)");
    };
    request.onsuccess = function openOnSuccess1(evt) {
        preamble(evt);
        shouldBeTrue("isAfterReload()");
        finishJSTest();
    };
    if (!isAfterReload())
        reload();
}

test();
