if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the deleteDatabase call and its interaction with open/setVersion");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();
    test1();
}

function test1() {
    debug("");
    debug("TEST: deleteDatabase blocked on open handles");
    evalAndLog("self.testname = 'test1'; self.ver = 1");
    evalAndLog("blockedEventFired = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function openOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h = event.target.result");

        h.onversionchange = function onVersionChange(evt) {
            preamble(evt);
            debug("old = " + JSON.stringify(event.target.version));
            debug("new = " + JSON.stringify(event.version));

            debug("scheduling timeout to close");
            // This setTimeout() is used to move the close call outside of the event dispatch
            // callback. Per spec, a blocked event should only fire if connections are still
            // open after the versionchange event dispatch is complete.
            // FIXME: There is potential for test flakiness in the timing between when the
            // timeout runs and when the blocked event is dispatched. Move the close call
            // into or after the blocked event handler.
            setTimeout(function timeoutCallback() {
                preamble();
                evalAndLog("h.close()");
            }, 0);
        };

        request = evalAndLog("indexedDB.deleteDatabase(dbname + testname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function deleteDatabaseOnBlocked(evt) {
            preamble(evt);
            evalAndLog("blockedEventFired = true");
        };
        request.onsuccess = function deleteDatabaseOnSuccess(evt) {
            preamble(evt);
            shouldBeTrue("blockedEventFired");
            test2();
        };
    };
}

function test2() {
    debug("");
    debug("TEST: deleteDatabase not blocked when handles close immediately");
    evalAndLog("self.testname = 'test2'; self.ver = 1");
    evalAndLog("blockedEventFired = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function openOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h = event.target.result");

        h.onversionchange = function onVersionChange(evt) {
            preamble(evt);
            debug("old = " + JSON.stringify(event.target.version));
            debug("new = " + JSON.stringify(event.version));
            evalAndLog("h.close()");
        };

        request = evalAndLog("indexedDB.deleteDatabase(dbname + testname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function deleteDatabaseOnBlocked(evt) {
            preamble(evt);
            evalAndLog("blockedEventFired = true");
        };
        request.onsuccess = function deleteDatabaseOnSuccess(evt) {
            preamble(evt);
            debug("FIXME: blocked event should not fire since connection closed. http://webkit.org/b/71130");
            shouldBeFalse("blockedEventFired");
            test3();
        };
    };
}

function test3() {
    debug("");
    debug("TEST: deleteDatabase is delayed if a VERSION_CHANGE transaction is running");
    evalAndLog("self.testname = 'test3'; self.ver = 1");
    evalAndLog("versionChangeComplete = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function openOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h = event.target.result");

        h.onversionchange = function onVersionChange(evt) {
            preamble(evt);
            debug("old = " + JSON.stringify(event.target.version));
            debug("new = " + JSON.stringify(event.version));

            debug("scheduling timeout to close");
            // This setTimeout() is used to move the close call outside of the event dispatch
            // callback. Per spec, a blocked event should only fire if connections are still
            // open after the versionchange event dispatch is complete.
            // FIXME: There is potential for test flakiness in the timing between when the
            // timeout runs and when the blocked event is dispatched. Move the close call
            // into or after the blocked event handler.
            setTimeout(function timeoutCallback() {
                preamble();
                evalAndLog("h.close()");
            }, 0);
        };

        request = evalAndLog("h.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function setVersionOnSuccess(evt) {
            preamble(evt);

            transaction = event.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function transactionOnComplete(evt) {
                preamble(evt);
                evalAndLog("versionChangeComplete = true");
            };
        };

        request = evalAndLog("indexedDB.deleteDatabase(dbname + testname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function deleteDatabaseOnBlocked(evt) {
            preamble(evt);
        };
        request.onsuccess = function deleteDatabaseOnSuccess(evt) {
            preamble(evt);
            shouldBeTrue("versionChangeComplete");
            test4();
        };
    };
}

function test4() {
    debug("");
    debug("TEST: Correct order when there are pending setVersion, delete and open calls.");
    evalAndLog("self.testname = 'test4'; self.ver = 1");
    evalAndLog("setVersionBlockedEventFired = false");
    evalAndLog("versionChangeComplete = false");
    evalAndLog("deleteDatabaseBlockedEventFired = false");
    evalAndLog("deleteDatabaseComplete = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function openOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h = event.target.result");

        h.onversionchange = function onVersionChange(evt) {
            preamble(evt);
            debug("old = " + JSON.stringify(event.target.version));
            debug("new = " + JSON.stringify(event.version));

            debug("scheduling timeout to close");
            // This setTimeout() is used to move the close call outside of the event dispatch
            // callback. Per spec, a blocked event should only fire if connections are still
            // open after the versionchange event dispatch is complete.
            // FIXME: There is potential for test flakiness in the timing between when the
            // timeout runs and when the blocked event is dispatched. Move the close call
            // into or after the blocked event handler.
            setTimeout(function timeoutCallback() {
                preamble();
                evalAndLog("h.close()");
            }, 0);
        };

        request = evalAndLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h2OpenOnSuccess(evt) {
            preamble(evt);
            h2 = event.target.result;

            h2.onversionchange = function h2OnVersionChange(evt) {
                preamble(evt);
                debug("old = " + JSON.stringify(event.target.version));
                debug("new = " + JSON.stringify(event.version));
            };

            request = evalAndLog("h.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function setVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("setVersionBlockedEventFired = true");

                request = evalAndLog("indexedDB.open(dbname + testname)");
                request.onblocked = unexpectedBlockedCallback;
                request.onerror = unexpectedErrorCallback;
                request.onsuccess = function h3OpenOnSuccess(evt) {
                    preamble(evt);
                    h3 = event.target.result;
                    h3.onversionchange = unexpectedVersionChangeCallback;

                    shouldBeTrue("versionChangeComplete");
                    shouldBeTrue("blockedEventFired");
                    shouldBeTrue("deleteDatabaseComplete");

                    finishJSTest();
                };

                evalAndLog("h2.close()");
            };
            request.onsuccess = function setVersionOnSuccess(evt) {
                preamble(evt);

                transaction = event.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function transactionOnComplete(evt) {
                    preamble(evt);
                    evalAndLog("versionChangeComplete = true");
                };
            };

            request = evalAndLog("indexedDB.deleteDatabase(dbname + testname)");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function deleteDatabaseOnBlocked(evt) {
                preamble(evt);
                evalAndLog("deleteDatabaseBlockedEventFired = true");
            };
            request.onsuccess = function deleteDatabaseOnSuccess(evt) {
                preamble(evt);
                evalAndLog("deleteDatabaseComplete = true");
            };
        };
    };
}

test();
