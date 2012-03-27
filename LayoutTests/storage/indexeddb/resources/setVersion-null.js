if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB with null version string");

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
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion(null);");
    request.onsuccess = postSetVersion;
    request.onerror = unexpectedErrorCallback;
}

function postSetVersion()
{
    shouldBe("db.version", "'null'");
    finishJSTest();
}


test();
