if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Regression test for WK136583 - ensure the versionchange event has the proper name");

indexedDBTest(prepareDatabase, upgradeDatabase);
function prepareDatabase(evt)
{
    db = event.target.result;
    evalAndLog("db.onversionchange = onVersionChange");
    evalAndLog("store = db.createObjectStore('store1')");
}

function upgradeDatabase(evt)
{
    db = evt.target.result;
    shouldBe("db.version", "1");
    evalAndLog("request = indexedDB.open(dbname, 2)");
    evalAndLog("db.onversionchange = onVersionChange");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = finishTest;
}

function onVersionChange(evt)
{
    preamble(evt);
    shouldBeEqualToString("event.type", "versionchange");
    db.close();
}

function finishTest(evt)
{
    finishJSTest();
}
