if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test behavior when the same connection calls setVersion twice");

function test() {
    removeVendorPrefixes();

    shouldBeNonNull("indexedDB");
    shouldBeNonNull("IDBTransaction");
    openDBConnection();
}

function openDBConnection()
{
    evalAndLog("self.state = 'starting'");
    var request = evalAndLog("indexedDB.open('two-versions-one-connection')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    self.db = evalAndLog("db = event.target.result");
    evalAndLog("self.state = 0");

    var versionChangeRequest1 = evalAndLog("db.setVersion('version 1')");
    versionChangeRequest1.onerror = unexpectedErrorCallback;
    versionChangeRequest1.onsuccess = inSetVersion1;

    // and concurrently...

    var versionChangeRequest2 = evalAndLog("db.setVersion('version 2')");
    versionChangeRequest2.onerror = unexpectedErrorCallback;
    versionChangeRequest2.onsuccess = inSetVersion2;
}

function inSetVersion1()
{
    debug("setVersion() #1 callback");
    trans = event.target.result;
    evalAndLog("self.store1 = db.createObjectStore('test-store1')");
    shouldBe("++self.state", "1");
    var req = evalAndLog("self.store1.put('aaa', 111)");
    req.onerror = unexpectedErrorCallback;
    req.onsuccess = function (e) {
        shouldBe("++self.state", "2");
        halfwayDone();
    };
}

function inSetVersion2()
{
    debug("setVersion() #2 callback");
    trans = event.target.result;
    shouldBe("++self.state", "4");
    evalAndLog("self.store2 = db.createObjectStore('test-store2')");

    var req = evalAndLog("self.store2.put('bbb', 222)");
    req.onerror = unexpectedErrorCallback;
    req.onsuccess = function (e) {
        shouldBe("++self.state", "5");
        halfwayDone();
    };
}

var counter = 0;
function halfwayDone() 
{
    counter += 1;
    if (counter < 2) {
        shouldBe("++self.state", "3");
        debug("halfway there..." );
    } else {
        shouldBe("++self.state", "6");
        checkResults();
    }
}

function checkResults() {
    shouldBeEqualToString("self.db.version", "version 2");
    trans.oncomplete = setVersionComplete;
}

function setVersionComplete()
{
    trans = evalAndLog("self.trans = db.transaction(['test-store1', 'test-store2'])");
    store = evalAndLog("self.store = self.trans.objectStore('test-store1')");
    req = evalAndLog("self.req = self.store.get(111)");
    req.onerror = unexpectedErrorCallback;
    req.onsuccess = function (e) {
        shouldBeEqualToString("event.target.result", "aaa");

        store = evalAndLog("self.store = self.trans.objectStore('test-store2')");
        req = evalAndLog("self.req = self.store.get(222)");
        req.onerror = unexpectedErrorCallback;
        req.onsuccess = function (e) {
            shouldBeEqualToString("event.target.result", "bbb");
            finishJSTest();
        };
    };
}

test();