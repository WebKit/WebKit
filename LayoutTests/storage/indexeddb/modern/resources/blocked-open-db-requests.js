description("Makes sure that an OpenDBRequest that would result in a version change is blocked because there are still open connections, it gets the blocked event. Also makes sure that request gets unblocked when the open connections close.");

indexedDBTest(prepareDatabase, versionChangeSuccessCallback);

blockedCount = 0;
receivedVersionChange = false;
function prepareDatabase()
{
    evalAndLog("connection = event.target.result;");
    evalAndLog("connection.onversionchange = firstVersionChange;");
    evalAndLog("objectStore = connection.createObjectStore('testObjectStore');");
}

function versionChangeSuccessCallback()
{
    debug("First version change transaction complete");
    evalAndLog("firstTransaction = connection.transaction('testObjectStore', 'readwrite');");
    evalAndLog("objectStore = firstTransaction.objectStore('testObjectStore');");
    evalAndLog("objectStorePutLoop();");

    evalAndLog("secondRequest = indexedDB.open(dbname, 2);");
    evalAndLog("secondRequest.onblocked = secondBlocked;");
    evalAndLog("secondRequest.onupgradeneeded = secondUpgradeNeeded;");
}

function firstVersionChange()
{
    evalAndLog("receivedVersionChange = true;");
}

function objectStorePutLoop()
{
    if (blockedCount == 1 && receivedVersionChange) {
        connection.close();
        return;
    }

    request = objectStore.put("bar", "foo");
    request.onsuccess = objectStorePutLoop;
}

function secondBlocked()
{
    evalAndLog("++blockedCount");
    blockedEvent = event;
    shouldBe("blockedEvent.oldVersion", "1");
    shouldBe("blockedEvent.newVersion", "2");
    shouldEvaluateTo("blockedEvent.target.readyState", "'pending'");
}

function secondUpgradeNeeded()
{
    debug("2nd upgradeNeeded");
    shouldBe("blockedCount", "1");
    shouldBe("event.target.result.version", "2");
    evalAndLog("event.target.result.close();");
    finishJSTest();
}
