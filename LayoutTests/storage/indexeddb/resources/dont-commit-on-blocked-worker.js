importScripts('../../../resources/js-test-pre.js');
importScripts('shared.js');

removeVendorPrefixes();

debug("opening database connection");
evalAndLog("request = indexedDB.open('dont-commit-on-blocked.html', 2)");
request.onerror = unexpectedErrorCallback;
evalAndLog("state = 'setversion'");
evalAndLog("request.onblocked = onSetVersionBlocked");
evalAndLog("request.onupgradeneeded = onSetVersionSuccess");
request.onsuccess = onTransactionComplete;
debug("spinning for 100ms to let events be queued but prevent dispatch");
var t = Date.now();
while ((Date.now() - t) < 100) {
}
debug("done spinning");

function onSetVersionBlocked(e)
{
  debug("");
  debug("onSetVersionBlocked():");
  shouldBeEqualToString("state", "setversion");
  evalAndLog("state = 'blocked'");
}

function onSetVersionSuccess(e)
{
  preamble(e);
  evalAndLog("db = event.target.result");
  db.onerror = unexpectedErrorCallback;
  shouldBeEqualToString("state", "blocked");
  evalAndLog("state = 'success'");

  debug("creating object store - will fail if transaction commited after blocked event");
  evalAndLog("db.createObjectStore('store2')");
}

function onTransactionComplete(e)
{
  debug("");
  debug("onTransactionComplete");

  shouldBeEqualToString("state", "success");

  shouldBe("Number(db.version)", "2");
  shouldBeTrue("db.objectStoreNames.contains('store1')");
  shouldBeTrue("db.objectStoreNames.contains('store2')");
  finishJSTest();
}
