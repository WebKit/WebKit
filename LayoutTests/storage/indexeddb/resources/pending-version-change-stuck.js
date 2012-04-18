if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

function test()
{
    removeVendorPrefixes();
    dbname = decodeURI(self.location.search.substring(1));
    evalAndLog("request = indexedDB.open(\"" + dbname + "\")");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        db = request.result;
        evalAndLog("request = db.setVersion(1)");
        request.onsuccess = unexpectedSuccessCallback;
        request.onblocked = function() {
            testPassed("worker received blocked event.");
            postMessage("gotblocked");
        };
    };
}

test();
