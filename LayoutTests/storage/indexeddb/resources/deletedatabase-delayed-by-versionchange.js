if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the order when there are pending setVersion, delete and open calls.");

indexedDBTest(null, h1OpenSuccess);
function h1OpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("setVersionBlockedEventFired = false");
    evalAndLog("versionChangeComplete = false");
    evalAndLog("deleteDatabaseBlockedEventFired = false");
    evalAndLog("deleteDatabaseComplete = false");

    evalAndLog("h1 = event.target.result");

    h1.onversionchange = function h1OnVersionChange(evt) {
        preamble(evt);
        shouldBe("event.target.version", "1");
        shouldBe("event.oldVersion", "1");
        shouldBe("event.newVersion", "2");

        h1.onversionchange = function h1SecondOnVersionChange(evt) {
            preamble(evt);
            shouldBe("event.target.version", "1");
            shouldBe("event.oldVersion", "1");
            shouldBeNull("event.newVersion");
        };
    };

    debug("Open h2:");
    request = evalAndLog("indexedDB.open(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function h2OpenSuccess(evt) {
        preamble(evt);
        h2 = event.target.result;

        h2.onversionchange = function h2OnVersionChange(evt) {
            preamble(evt);
            shouldBe("event.target.version", "1");
            shouldBe("event.oldVersion", "1");
            shouldBe("event.newVersion", "2");
        };

        debug("Try to open h3:");
        request = evalAndLog("indexedDB.open(dbname, 2)");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function h3OpenSuccess(evt) {
            preamble(evt);
        }
        request.onblocked = function h3Blocked(evt) {
            preamble(evt);
            evalAndLog("setVersionBlockedEventFired = true");

            debug("Try to open h4:");
            request = evalAndLog("indexedDB.open(dbname)");
            request.onblocked = unexpectedBlockedCallback;
            request.onerror = unexpectedErrorCallback;
            request.onsuccess = function h4OpenSuccess(evt) {
                preamble(evt);
                h4 = event.target.result;
                h4.onversionchange = unexpectedVersionChangeCallback;

                shouldBeTrue("setVersionBlockedEventFired");
                shouldBeTrue("versionChangeComplete");
                shouldBeTrue("deleteDatabaseBlockedEventFired");
                shouldBeTrue("deleteDatabaseComplete");

                finishJSTest();
            };

            evalAndLog("h2.close()");
        };
        request.onupgradeneeded = function h3OnUpgradeneeded(evt) {
            preamble(evt);

            transaction = event.target.transaction;
            transaction.onabort = unexpectedAbortCallback;
            transaction.oncomplete = function transactionOnComplete(evt) {
                preamble(evt);
                evalAndLog("versionChangeComplete = true");
            };
        };

        request = evalAndLog("indexedDB.deleteDatabase(dbname)");
        request.onerror = unexpectedErrorCallback;
        request.onblocked = function deleteDatabaseOnBlocked(evt) {
            preamble(evt);
            evalAndLog("deleteDatabaseBlockedEventFired = true");

            evalAndLog("h1.close()");
        };
        request.onsuccess = function deleteDatabaseOnSuccess(evt) {
            preamble(evt);
            evalAndLog("deleteDatabaseComplete = true");
        };
    };
}
