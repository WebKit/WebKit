if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that specifying a version when opening a non-existent db causes an upgradeneeded event and that the version number is set correctly.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function deleteSuccess(evt) {
    debug("Should trigger:");
    debug("4.1.4: If no database with the given name from the origin origin was found, or if it was deleted during the previous step, then create a database with name name, with 0 as version, and with no object stores. Let db be the new database.");
    debug("4.1.6: Create a new connection to db and let connection represent it.");
    debug("4.1.7: If the version of db is lower than version, then run the steps for running a \"versionchange\" transaction using connection, version, request and upgrade callback.");
    debug("4.1.9: Return connection.");

    evalAndLog("request = indexedDB.open(dbname, 7)");
    shouldBeEqualToString("String(request)", "[object IDBOpenDBRequest]");
    request.onsuccess = openSuccess;
    request.onupgradeneeded = upgradeNeeded;
    request.onblocked = unexpectedBlockedCallback;
    debug("");
}

var sawUpgradeNeeded = false;
var sawTransactionComplete = false;
function upgradeNeeded(evt)
{
    event = evt;
    testPassed("In the upgradeneeded event hander:");
    debug("Test 4.8.9.1:");
    evalAndLog("db = event.target.result");
    shouldBeEqualToString("String(db)", "[object IDBDatabase]");
    debug("Test 4.8.9.3:");
    shouldBe("event.oldVersion", "0");
    shouldBe("event.newVersion", "7");
    shouldBeEqualToString("event.target.readyState", "done");
    debug("Test 4.1.4:");
    shouldBe("db.name", "dbname");
    shouldBe("db.version", "7");
    shouldBe("db.objectStoreNames.length", "0");

    evalAndLog("transaction = event.target.transaction");
    shouldBeEqualToString("String(transaction)", "[object IDBTransaction]");
    evalAndLog('os = db.createObjectStore("someOS")');
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function(e) {
      evalAndLog("sawTransactionComplete = true");
    }
    sawUpgradeNeeded = true;
}

function openSuccess(evt)
{
    event = evt;
    debug("");
    debug("openSuccess():");
    debug("Test 4.1.7, that a versionchange transaction was run.");
    shouldBe("sawUpgradeNeeded", "true");
    shouldBeTrue("sawTransactionComplete");
    db = evalAndLog("db = event.target.result");
    debug("Kind of test 4.1.9:");
    shouldBeEqualToString("String(db)", "[object IDBDatabase]");
    shouldBe('db.version', "7");
    finishJSTest();
}

test();
