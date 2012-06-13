if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Three setVersion calls are queued correctly - exercises an otherwise-untested code path");

function test()
{
    removeVendorPrefixes();
    openDBConnection();
}

function openDBConnection()
{
    request = evalAndLog("indexedDB.open('three-setversion-calls')");
    request.onsuccess = openSuccess1;
    request.onerror = unexpectedErrorCallback;
}

function successHandler(id)
{
  return function(e) {
    trans = e.target.result;
    debug("setVersion " + id);
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = function(evt) {
      debug("setVersion " + id + " complete");
      if (id == kNumVersionCalls - 1) {
        evalAndLog("finishedAllSetVersions = true");
        connection.close();
      }
    }
  }
}

finishedAllSetVersions = false;
kNumVersionCalls = 3;
function openSuccess1(e)
{
    connection = e.target.result;
    for (var i = 0; i < kNumVersionCalls; i++) {
      vcRequest = evalAndLog('connection.setVersion("version ' + i + '")');
      vcRequest.onblocked = unexpectedBlockedCallback;
      vcRequest.onerror = unexpectedErrorCallback;
      vcRequest.onsuccess = successHandler(i);
      if (!i) {
        deleteRequest = evalAndLog('indexedDB.deleteDatabase("three-setversion-calls")');
        deleteRequest.onerror = unexpectedErrorCallback;
        deleteRequest.onsuccess = function() {
          debug("Successfully deleted database");
          shouldBeTrue("finishedAllSetVersions");
          finishJSTest();
        };
      }
    }
}

test();
