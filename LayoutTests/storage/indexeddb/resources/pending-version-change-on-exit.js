if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

function test() {
  removeVendorPrefixes();
  dbname = "pending-version-change-on-exit";
  evalAndLog("request = indexedDB.open(\"" + dbname + "\")");
  request.onsuccess = function(e) {
    db = request.result;
    evalAndLog("request = db.setVersion(1)");
    request.onsuccess = unexpectedSuccessCallback;
    request.onblocked = function() {
      testPassed("worker received blocked event.");
      finishJSTest();
    };
  };
}

test();
