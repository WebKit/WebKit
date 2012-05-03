// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_setVersion_events.html
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../fast/js/resources/js-test-pre.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB's triggering versionchange event");

function test()
{
    removeVendorPrefixes();

    name = self.location.pathname;
    description = "My Test Database";
    request = evalAndLog("indexedDB.open(name, description)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db1 = evalAndLog("db1 = event.target.result");
    versionChangeEventCount = evalAndLog("versionChangeEventCount = 0;");
    debug("db1 will be open when we call db2.setVersion, which will trigger versionchange on db1");
    db1.onversionchange = function(event) {
        shouldBe("event.target", "db1");
        shouldBe("event.version", "'1'");
        versionChangeEventCount++;
        shouldBe("versionChangeEventCount", "1");
        debug("close db1 now so versionchange doesn't trigger on db1 again when we call db3.setVersion");
        evalAndLog("db1.close();");
    };
    request = evalAndLog("indexedDB.open(name, description)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = openSuccess2;
}

function openSuccess2()
{
    db2 = evalAndLog("db2 = event.target.result");
    debug("db2 will be open when we call db3.setVersion, which will trigger versionchange on db2");
    db2.onversionchange = function(event) {
        t = event.target;
        shouldBe("event.target", "db2");
        shouldBe("event.target.version", "'1'");
        shouldBe("event.version", "'2'");
        versionChangeEventCount++;
        shouldBe("versionChangeEventCount", "2");
        debug("don't close db2, so it will block db3.setVersion");
    }
    request = evalAndLog("request = db2.setVersion('1')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = postSetVersion;
    request.onblocked = function(event) {
        testPassed("db2 received blocked event");
    }
}

function postSetVersion()
{
    shouldBe("event.target.result", "event.target.transaction");
    shouldBe("event.target.transaction.mode", "'versionchange'");
    shouldBe("db2.version", "'1'");

    request = evalAndLog("request = indexedDB.open(name, description);");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = openSuccess3;
}

function openSuccess3()
{
    db3 = evalAndLog("db3 = event.target.result;");
    request = evalAndLog("request = db3.setVersion('2');");
    request.onerror = unexpectedErrorCallback;
    debug("this request will block because db2 is still open");
    request.onblocked = function(event) {
        shouldBe("event.target.source", "db3");
        shouldBe("event.target.source.version", "'1'");
        shouldBe("event.version", "'2'");
        versionChangeEventCount++;
        debug("close db2 now so onsuccess will be called");
        evalAndLog("db2.close();");
    }
    request.onsuccess = postSetVersion2;
}

function postSetVersion2()
{
    shouldBe("event.target.result", "event.target.transaction");
    shouldBe("event.target.transaction.mode", "'versionchange'");
    shouldBe("db3.version", "'2'");
    shouldBe("versionChangeEventCount", "3");
    finishJSTest();
}

test();