if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

if (window.testRunner) {
    testRunner.setOriginQuotaRatioEnabled(false);
    testRunner.setAllowStorageQuotaIncrease(false);
}

var quota = 400 * 1024; // default quota for testing.
description("This test makes sure that storage of indexedDB does not grow unboundedly.");

indexedDBTest(prepareDatabase, onOpenSuccess);

function prepareDatabase(event)
{
    preamble(event);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
}

function onOpenSuccess(event)
{
    preamble(event);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.transaction('store', 'readwrite').objectStore('store')");

    // Small add should succeed.
    evalAndLog("addCount = 0");
    for (var i = 1; i <= 10; i ++)
        evalAndLog("store.add(new Uint8Array(1), " + i + ").onsuccess = ()=> { ++addCount; }");

	// Big add should fail.
    evalAndLog("request = store.add(new Uint8Array(" + (quota + 1) + "), 0)");
    request.onerror = function(event) {
        shouldBe("addCount", "10");
        shouldBeTrue("'error' in request");
        shouldBe("request.error.code", "DOMException.QUOTA_EXCEEDED_ERR");
        shouldBeEqualToString("request.error.name", "QuotaExceededError");
        finishJSTest();
    }

    request.onsuccess = function(event) {
        testFailed("Add operation should fail because storage limit is reached, but succeeded.");
        finishJSTest();
    }
}
