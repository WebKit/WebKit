if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test interleaved open/close/setVersion calls in various permutations");

function test() {
    removeVendorPrefixes();
    setDBNameFromPath();
    test1();
}

function test1() {
    debug("");
    debug("TEST: setVersion blocked on open handles");
    evalAndLog("testname = 'test1'");
    evalAndLog("ver = 1");
    evalAndLog("blockedEventFired = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = unexpectedVersionChangeCallback;

        request = evalAndLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h2OpenOnSuccess(evt) {
            preamble(evt);

            evalAndLog("h2 = event.target.result");
            h2.onversionchange = function h2OnVersionChange(evt) {
                preamble(evt);
                debug("old = " + JSON.stringify(event.target.version));
                debug("new = " + JSON.stringify(event.version));

                debug("scheduling timeout to close h2");
                // This setTimeout() is used to move the close call outside of the event dispatch
                // callback. Per spec, a blocked event should only fire if connections are still
                // open after the versionchange event dispatch is complete.
                // FIXME: There is potential for test flakiness in the timing between when the
                // timeout runs and when the blocked event is dispatched. Move the close call
                // into or after the blocked event handler.
                setTimeout(function timeoutCallback() {
                    preamble();
                    evalAndLog("h2.close()");
                }, 0);
            };

            request = evalAndLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function h1SetVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("blockedEventFired = true");
            };
            request.onsuccess = function h1SetVersionOnSuccess(evt) {
                preamble(evt);

                transaction = event.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function transactionOnComplete(evt) {
                    preamble(evt);
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
    evalAndLog("testname = 'test2'");
    evalAndLog("ver = 1");
    evalAndLog("blockedEventFired = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = unexpectedVersionChangeCallback;

        request = evalAndLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h1OpenOnSuccess(evt) {
            preamble(evt);
            evalAndLog("h2 = event.target.result");

            h2.onversionchange = function h2OnVersionChange(evt) {
                preamble(evt);
                debug("old = " + JSON.stringify(event.target.version));
                debug("new = " + JSON.stringify(event.version));

                evalAndLog("h2.close()");
            };

            request = evalAndLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function h1SetVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("blockedEventFired = true");
            };
            request.onsuccess = function h1SetVersionOnSuccess(evt) {
                preamble(evt);

                transaction = event.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function transactionOnComplete(evt) {
                    preamble(evt);
                    debug("FIXME: blocked should not have fired since connection closed; http://webkit.org/b/71130");
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
    evalAndLog("testname = 'test3'");
    evalAndLog("ver = 1");
    evalAndLog("blockedEventFired = false");
    evalAndLog("versionChangeComplete = false");
    evalAndLog("errorEventFired = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = function h1OnVersionChange(evt) {
            preamble(evt);
            debug("old = " + JSON.stringify(event.target.version));
            debug("new = " + JSON.stringify(event.version));
        };

        request = evalAndLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h2OpenOnSuccess(evt) {
            preamble(evt);
            evalAndLog("h2 = event.target.result");

            h2.onversionchange = function h2OnVersionChange(evt) {
                preamble(evt);
                debug("old = " + JSON.stringify(event.target.version));
                debug("new = " + JSON.stringify(event.version));
            };

            request = evalAndLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function h1SetVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("blockedEventFired = true");
            };
            request.onsuccess = function h1SetVersionOnSuccess(evt) {
                preamble(evt);

                transaction = event.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function transactionOnComplete(evt) {
                    preamble(evt);
                    evalAndLog("versionChangeComplete = true");
                };
            };

            request = evalAndLog("h2.setVersion(String(ver++))");
            request.onblocked = function h2SetVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("h2.close()");

                request = evalAndLog("indexedDB.open(dbname + testname)");
                request.onblocked = unexpectedBlockedCallback;
                request.onerror = unexpectedErrorCallback;
                request.onsuccess = function h3OpenOnSuccess(evt) {
                    preamble(event);
                    shouldBeTrue("blockedEventFired");
                    shouldBeTrue("versionChangeComplete");
                    shouldBeTrue("errorEventFired");
                    test4();
                };
            };
            request.onerror = function h2SetVersionOnError(evt) {
                preamble(evt);
                evalAndLog("errorEventFired = true");
            };
        };
    };
}

function test4() {
    debug("");
    debug("TEST: open and setVersion blocked if a VERSION_CHANGE transaction is running - just close");
    evalAndLog("testname = 'test4'");
    evalAndLog("ver = 1");
    evalAndLog("blockedEventFired = false");
    evalAndLog("versionChangeComplete = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = unexpectedVersionChangeCallback;

        request = evalAndLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h2OpenOnSuccess(evt) {
            preamble(evt);
            evalAndLog("h2 = event.target.result");

            h2.onversionchange = unexpectedVersionChangeCallback;

            request = evalAndLog("h1.setVersion(String(ver++))");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = function h1SetVersionOnBlocked(evt) {
                preamble(evt);
                evalAndLog("blockedEventFired = true");
            };
            request.onsuccess = function h1SetVersionOnSuccess(evt) {
                preamble(evt);

                transaction = event.target.result;
                transaction.onabort = unexpectedAbortCallback;
                transaction.oncomplete = function transactionOnComplete(evt) {
                    preamble(evt);
                    evalAndLog("versionChangeComplete = true");
                };
            };

            request = evalAndLog("indexedDB.open(dbname + testname)");
            request.onblocked = unexpectedBlockedCallback;
            request.onerror = unexpectedErrorCallback;
            request.onsuccess = function h3OpenOnSuccess(evt) {
                preamble(evt);
                debug("FIXME: blocked should not fire as connection was closed. http://webkit.org/b/71130");
                shouldBeFalse("blockedEventFired");
                shouldBeTrue("versionChangeComplete");
                test5();
            };

            evalAndLog("h2.close()");
        };
    };
}

function test5() {
    debug("");
    debug("TEST: open blocked if a VERSION_CHANGE transaction is running");
    evalAndLog("testname = 'test5'");
    evalAndLog("ver = 1");
    evalAndLog("versionChangeComplete = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = unexpectedVersionChangeCallback;

        request = evalAndLog("h1.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function h1SetVersionOnSuccess(evt) {
            preamble(evt);

            transaction = event.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function transactionOnComplete(evt) {
                preamble(evt);
                evalAndLog("versionChangeComplete = true");
            };
        };

        request = evalAndLog("indexedDB.open(dbname + testname)");
        request.onblocked = unexpectedBlockedCallback;
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h2OpenOnSuccess(evt) {
            preamble(evt);
            evalAndLog("h2 = event.target.result");

            h2.onversionchange = unexpectedVersionChangeCallback;
            shouldBeTrue("versionChangeComplete");
            test6();
        };
    };
}

function test6() {
    debug("");
    debug("TEST: two setVersions from the same connection");
    evalAndLog("testname = 'test6'");
    evalAndLog("ver = 1");
    evalAndLog("versionChangeComplete = false");

    request = evalAndLog("indexedDB.open(dbname + testname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h1OpenOnSuccess(evt) {
        preamble(evt);
        evalAndLog("h1 = event.target.result");

        h1.onversionchange = unexpectedVersionChangeCallback;

        request = evalAndLog("h1.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function h1SetVersionOnSuccess1(evt) {
            preamble(event);

            transaction = event.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function transactionOnComplete1(evt) {
                preamble(evt);
                debug("half done");
                evalAndLog("versionChangeComplete = true");
            };
        };


        request = evalAndLog("h1.setVersion(String(ver++))");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onsuccess = function h1SetVersionOnSuccess2(evt) {
            preamble(event);

            transaction = event.target.result;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function transactionOnComplete2(evt) {
                preamble(evt);
                shouldBeTrue("versionChangeComplete");
                finishJSTest();
            };
        };
    };
}

test();
