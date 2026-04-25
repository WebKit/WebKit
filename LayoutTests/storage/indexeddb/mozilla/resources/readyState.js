// original test:
// http://mxr.mozilla.org/mozilla2.0/source/dom/indexedDB/test/test_request_readyState.html?force=1
// license of original test:
// " Any copyright is dedicated to the Public Domain.
//   http://creativecommons.org/publicdomain/zero/1.0/ "

if (this.importScripts) {
    importScripts('../../../../resources/js-test.js');
    importScripts('../../resources/shared.js');
}

description("Test IndexedDB readyState property during different operations");

indexedDBTest(prepareDatabase, checkReadyStateInOpen);
function prepareDatabase()
{
    db = event.target.result;
    openRequest = event.target;
    shouldBe("openRequest.readyState", "'done'");

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    key = evalAndLog("key = 10;");
    request = evalAndLog("request = objectStore.add({}, key);");
    shouldBe("request.readyState", "'pending'");
    request.onsuccess = getRecord;
    request.onerror = unexpectedErrorCallback;
}

function getRecord()
{
    debug("getRecord():");
    shouldBe("request.readyState", "'done'");
    shouldBe("event.target.result", "key");
    request = evalAndLog("request = objectStore.get(key);");
    shouldBe("request.readyState", "'pending'");
    request.onsuccess = finalCheck;
    request.onerror = unexpectedErrorCallback;
}

function finalCheck()
{
    debug("finalCheck():");
    shouldBe("request.readyState", "'done'");
    shouldBeFalse("event.target.result == null");
}

function checkReadyStateInOpen()
{
    debug("checkReadyStateInOpen():");
    shouldBe("event.target", "openRequest");
    shouldBeEqualToString("openRequest.readyState", "done");
    finishJSTest();
}
