if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

function test()
{
    removeVendorPrefixes();
    dbname = decodeURIComponent(self.location.search.substring(1));
    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = unexpectedSuccessCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onblocked = function() {
        testPassed("worker received blocked event.");
        postMessage("gotblocked");
    };
}

test();
