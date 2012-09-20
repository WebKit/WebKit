if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test interleaved open/close/setVersion calls in various permutations");

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
    debug("TEST: setVersion blocked on open handles");
    evalAndLog("self.testname = 'test1'; self.ver = 1");
    evalNoLog("blockedEventFired = false");

    debug("'h1.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h1.open.onsuccess'");
        h1 = e.target.result;

        h1.onversionchange = unexpectedVersionChangeCallback;

        debug("'h2.open'");
        request = evalNoLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(e) {
            debug("'h2.open.onsuccess'");
            h2 = e.target.result;

            h2.onversionchange = function() {
                debug("'h2.onversionchange'");
                debug("    h2 closing, but not immediately");
                // This setTimeout() is used to move the close call outside of the event dispatch
                // callback. Per spec, a blocked event should only fire if connections are still
                // open after the versionchange event dispatch is complete.
                // FIXME: There is potential for test flakiness in the timing between when the
                // timeout runs and when the blocked event is dispatched. Move the close call
                // into or after the blocked event handler.
                setTimeout(function() {
                    debug("'h2.close'");
                    evalNoLog("h2.close()");
                }, 0);
            };

            debug("'h1.setVersion'");
            request = evalNoLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function() {
                debug("'h1.setVersion.onblocked'");
                evalNoLog("blockedEventFired = true");
            };
            request.onsuccess = function(e) {
                debug("'h1.setVersion.onsuccess'");

                transaction = e.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function() {
                    debug("'h1.setVersion.transaction-complete'");
                    shouldBeTrue("blockedEventFired");
                    test2();
                };
            };
        };
    };
}

function test2() {
    debug("");
    debug("TEST: setVersion not blocked if handle closed immediately");
    evalAndLog("self.testname = 'test2'; self.ver = 1");
    evalNoLog("blockedEventFired = false");

    debug("'h1.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h1.open.onsuccess'");
        h1 = e.target.result;

        h1.onversionchange = unexpectedVersionChangeCallback;

        debug("'h2.open'");
        request = evalNoLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(e) {
            debug("'h2.open.onsuccess'");
            h2 = e.target.result;

            h2.onversionchange = function() {
                debug("'h2.onversionchange'");
                debug("    h2 closing immediately");
                debug("'h2.close'");
                evalNoLog("h2.close()");
            };

            debug("'h1.setVersion'");
            request = evalNoLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function() {
                debug("'h1.setVersion.onblocked'");
                evalNoLog("blockedEventFired = true");
            };
            request.onsuccess = function(e) {
                debug("'h1.setVersion.onsuccess'");

                transaction = e.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function() {
                    debug("'h1.setVersion.transaction-complete'");
                    debug("NOTE: Will FAIL with extra bogus h1.setVersion.onblocked step; https://bugs.webkit.org/show_bug.cgi?id=71130");
                    shouldBeFalse("blockedEventFired");
                    test3();
                };
            };
        };
    };
}

function test3() {
    debug("");
    debug("TEST: open and setVersion blocked if a VERSION_CHANGE transaction is running - close when blocked");
    evalAndLog("self.testname = 'test3'; self.ver = 1");
    evalNoLog("blockedEventFired = false");
    evalNoLog("versionChangeComplete = false");

    debug("'h1.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h1.open.onsuccess'");
        h1 = e.target.result;

        h1.onversionchange = function() {
            debug("'h1.onversionchange'");
        };

        debug("'h2.open'");
        request = evalNoLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(e) {
            debug("'h2.open.onsuccess'");
            h2 = e.target.result;

            h2.onversionchange = function() {
                debug("'h2.onversionchange'");
            };

            debug("'h1.setVersion'");
            request = evalNoLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function() {
                debug("'h1.setVersion.onblocked'");
                evalNoLog("blockedEventFired = true");
            };
            request.onsuccess = function(e) {
                debug("'h1.setVersion.onsuccess'");

                transaction = e.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function() {
                    debug("'h1.setVersion.transaction-complete'");
                    evalNoLog("versionChangeComplete = true");
                };
            };

            debug("'h2.setVersion'");
            request = evalNoLog("h2.setVersion(String(ver++))");
            request.onblocked = function() {
                debug("'h2.setVersion.onblocked'");
                debug("    h2 blocked so closing");
                debug("'h2.close'");
                h2.close();

                debug("'h3.open'");
                request = evalNoLog("indexedDB.open(dbname + testname)");
                request.onblocked = unexpectedBlockedCallback;
                request.onerror = unexpectedErrorCallback;
                request.onsuccess = function() {
                    debug("'h3.open.onsuccess'");
                    shouldBeTrue("blockedEventFired");
                    shouldBeTrue("versionChangeComplete");
                    test4();
                };
            };
            request.onerror = function() {
                debug("'h2.setVersion.onerror'");
            };
        };
    };
}

function test4() {
    debug("");
    debug("TEST: open and setVersion blocked if a VERSION_CHANGE transaction is running - just close");
    evalAndLog("self.testname = 'test4'; self.ver = 1");
    evalNoLog("blockedEventFired = false");
    evalNoLog("versionChangeComplete = false");

    debug("'h1.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h1.open.onsuccess'");
        h1 = e.target.result;

        h1.onversionchange = unexpectedVersionChangeCallback;

        debug("'h2.open'");
        request = evalNoLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(e) {
            debug("'h2.open.onsuccess'");
            h2 = e.target.result;

            h2.onversionchange = unexpectedVersionChangeCallback;

            debug("'h1.setVersion'");
            request = evalNoLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function() {
                debug("'h1.setVersion.onblocked'");
                evalNoLog("blockedEventFired = true");
            };
            request.onsuccess = function(e) {
                debug("'h1.setVersion.onsuccess'");

                transaction = e.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function() {
                    debug("'h1.setVersion.transaction-complete'");
                    evalNoLog("versionChangeComplete = true");
                };
            };

            debug("'h3.open'");
            request = evalNoLog("indexedDB.open(dbname + testname)");
            request.onblocked = unexpectedBlockedCallback;
            request.onerror = unexpectedErrorCallback;
            request.onsuccess = function() {
                debug("'h3.open.onsuccess'");
                debug("NOTE: Will FAIL with extra bogus h1.setVersion.onblocked step; https://bugs.webkit.org/show_bug.cgi?id=71130");
                shouldBeFalse("blockedEventFired");
                shouldBeTrue("versionChangeComplete");
                test5();
            };

            debug("'h2.close'");
            evalNoLog("h2.close()");
        };
    };
}

function test5() {
    debug("");
    debug("TEST: open blocked if a VERSION_CHANGE transaction is running");
    evalAndLog("self.testname = 'test5'; self.ver = 1");
    evalNoLog("versionChangeComplete = false");

    debug("'h1.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h1.open.onsuccess'");
        h1 = e.target.result;

        h1.onversionchange = unexpectedVersionChangeCallback;

        debug("'h1.setVersion'");
        request = evalNoLog("h1.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function(e) {
            debug("'h1.setVersion.onsuccess'");

            transaction = e.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function() {
                debug("'h1.setVersion.transaction-complete'");
                evalNoLog("versionChangeComplete = true");
            };
        };

        debug("'h2.open'");
        request = evalNoLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function(e) {
            debug("'h2.open.onsuccess'");
            h2 = e.target.result;
            h2.onversionchange = unexpectedVersionChangeCallback;
            shouldBeTrue("versionChangeComplete");
            test6();
        };
    };
}

function test6() {
    debug("");
    debug("TEST: two setVersions from the same connection");
    evalAndLog("self.testname = 'test6'; self.ver = 1");
    evalNoLog("versionChangeComplete = false");

    debug("'h1.open'");
    request = evalNoLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function(e) {
        debug("'h1.open.onsuccess'");
        h1 = e.target.result;
        h1.onversionchange = unexpectedVersionChangeCallback;

        debug("'h1.setVersion'");
        request = evalNoLog("h1.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function(e) {
            debug("'h1.setVersion.onsuccess'");

            transaction = e.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function() {
                debug("'h1.setVersion.transaction-complete'");
                debug("half done");
                evalNoLog("versionChangeComplete = true");
            };
        };


        debug("'h1.setVersion'");
        request = evalNoLog("h1.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function(e) {
            debug("'h1.setVersion.onsuccess'");

            transaction = e.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function() {
                debug("'h1.setVersion.transaction-complete'");
                shouldBeTrue("versionChangeComplete");
                finishJSTest();
            };
        };
    };
}

test();
