if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the deleteDatabase call and its interaction with open/setVersion");

// This is used to get the safety of evalAndLog() but produce no output
// like earlier iterations of this layout test.
// FIXME: Just use evalAndLog() once test conversion is complete.
function evalNoLog(_a)
{
  if (typeof _a != "string")
    debug("WARN: evalNoLog() expects a string argument");

  var _av;
  try {
     _av = eval(_a);
  } catch (e) {
    testFailed(_a + " threw exception " + e);
  }
  return _av;
}

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();
    test1();
}

function test1() {
    debug("");
    debug("TEST: deleteDatabase blocked on open handles");
    evalAndLog("self.testname = 'test1'; self.ver = 1");
    evalNoLog("blockedEventFired = false");

    debug("'h.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h.open.onsuccess'");
        h = e.target.result;

        h.onversionchange = function(e) {
            debug("'h.onversionchange'");
            debug("    in versionchange, old = " + JSON.stringify(e.target.version) + " new = " + JSON.stringify(e.version));
            debug("    h closing, but not immediately");
            // This setTimeout() is used to move the close call outside of the event dispatch
            // callback. Per spec, a blocked event should only fire if connections are still
            // open after the versionchange event dispatch is complete.
            // FIXME: There is potential for test flakiness in the timing between when the
            // timeout runs and when the blocked event is dispatched. Move the close call
            // into or after the blocked event handler.
            setTimeout(function() {
                debug("'h.close'");
                h.close();
            }, 0);
        };

        debug("'deleteDatabase()'");
        request = evalNoLog("indexedDB.deleteDatabase(dbname + testname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function(e) {
            debug("'deleteDatabase().onblocked'");
            evalNoLog("blockedEventFired = true");
        };
        request.onsuccess = function(e) {
            debug("'deleteDatabase().onsuccess'");
            shouldBeTrue("blockedEventFired");
            test2();
        };
    };
}

function test2() {
    debug("");
    debug("TEST: deleteDatabase not blocked when handles close immediately");
    evalAndLog("self.testname = 'test2'; self.ver = 1");
    evalNoLog("blockedEventFired = false");

    debug("'h.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h.open.onsuccess'");
        h = e.target.result;

        h.onversionchange = function(e) {
            debug("'h.onversionchange'");
            debug("    in versionchange, old = " + JSON.stringify(e.target.version) + " new = " + JSON.stringify(e.version));
            debug("    h closing immediately");
            debug("'h.close'");
            h.close();
        };

        debug("'deleteDatabase()'");
        request = evalNoLog("indexedDB.deleteDatabase(dbname + testname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function(e) {
            debug("'deleteDatabase().onblocked'");
            evalNoLog("blockedEventFired = true");
        };
        request.onsuccess = function(e) {
            debug("'deleteDatabase().onsuccess'");
            debug("NOTE: Will FAIL with extra bogus deleteDatabase().onblocked step; https://bugs.webkit.org/show_bug.cgi?id=71130");
            shouldBeFalse("blockedEventFired");
            test3();
        };
    };
}

function test3() {
    debug("");
    debug("TEST: deleteDatabase is delayed if a VERSION_CHANGE transaction is running");
    evalAndLog("self.testname = 'test3'; self.ver = 1");
    evalNoLog("versionChangeComplete = false");

    debug("'h.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h.open.onsuccess'");
        h = e.target.result;

        h.onversionchange = function(e) {
            debug("'h.onversionchange'");
            debug("    in versionchange, old = " + JSON.stringify(e.target.version) + " new = " + JSON.stringify(e.version));
            debug("    h closing, but not immediately");
            // This setTimeout() is used to move the close call outside of the event dispatch
            // callback. Per spec, a blocked event should only fire if connections are still
            // open after the versionchange event dispatch is complete.
            // FIXME: There is potential for test flakiness in the timing between when the
            // timeout runs and when the blocked event is dispatched. Move the close call
            // into or after the blocked event handler.
            setTimeout(function() {
                debug("'h.close'");
                h.close();
            }, 0);
        };

        debug("'h.setVersion'");
        request = evalNoLog("h.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function(e) {
            debug("'h.setVersion.onsuccess'");

            transaction = e.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function() {
                debug("'h.setVersion.transaction-complete'");
                evalNoLog("versionChangeComplete = true");
            };
        };

        debug("'deleteDatabase()'");
        request = evalNoLog("indexedDB.deleteDatabase(dbname + testname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function(e) {
            debug("'deleteDatabase().onblocked'");
        };
        request.onsuccess = function(e) {
            debug("'deleteDatabase().onsuccess'");
            shouldBeTrue("versionChangeComplete");
            test4();
        };
    };
}

function test4() {
    debug("");
    debug("TEST: Correct order when there are pending setVersion, delete and open calls.");
    evalAndLog("self.testname = 'test4'; self.ver = 1");
    evalNoLog("setVersionBlockedEventFired = false");
    evalNoLog("versionChangeComplete = false");
    evalNoLog("deleteDatabaseBlockedEventFired = false");
    evalNoLog("deleteDatabaseComplete = false");

    debug("'h.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h.open.onsuccess'");
        h = e.target.result;

        h.onversionchange = function(e) {
            debug("'h.onversionchange'");
            debug("    in versionchange, old = " + JSON.stringify(e.target.version) + " new = " + JSON.stringify(e.version));
            debug("    h closing, but not immediately");
            // This setTimeout() is used to move the close call outside of the event dispatch
            // callback. Per spec, a blocked event should only fire if connections are still
            // open after the versionchange event dispatch is complete.
            // FIXME: There is potential for test flakiness in the timing between when the
            // timeout runs and when the blocked event is dispatched. Move the close call
            // into or after the blocked event handler.
            setTimeout(function() {
                debug("'h.close'");
                h.close();
            }, 0);
        };

        debug("'h2.open'");
        request = evalNoLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(e) {
            debug("'h2.open.onsuccess'");
            h2 = e.target.result;

            h2.onversionchange = function(e) {
                debug("'h2.onversionchange'");
                debug("    in versionchange, old = " + JSON.stringify(e.target.version) + " new = " + JSON.stringify(e.version));
            };

            debug("'h.setVersion'");
            request = evalNoLog("h.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function() {
                debug("'h.setVersion.onblocked'");
                evalNoLog("setVersionBlockedEventFired = true");

                debug("'h3.open'");
                request = evalNoLog("indexedDB.open(dbname + testname)");
                request.onblocked = unexpectedBlockedCallback;
                request.onerror = unexpectedErrorCallback;
                request.onsuccess = function(e) {
                    debug("'h3.open.onsuccess'");
                    h3 = e.target.result;
                    h3.onversionchange = unexpectedVersionChangeCallback;

                    shouldBeTrue("versionChangeComplete");
                    shouldBeTrue("blockedEventFired");
                    shouldBeTrue("deleteDatabaseComplete");

                    finishJSTest();
                };

                debug("'h2.close'");
                h2.close();
            };
            request.onsuccess = function(e) {
                debug("'h.setVersion.onsuccess'");

                transaction = e.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function() {
                    debug("'h.setVersion.transaction-complete'");
                    evalNoLog("versionChangeComplete = true");
                };
            };

            debug("'deleteDatabase()'");
            request = evalNoLog("indexedDB.deleteDatabase(dbname + testname)");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function(e) {
                debug("'deleteDatabase().onblocked'");
                evalNoLog("deleteDatabaseBlockedEventFired = true");
            };
            request.onsuccess = function(e) {
                debug("'deleteDatabase().onsuccess'");
                evalNoLog("deleteDatabaseComplete = true");
            };
        };
    };
}

test();
