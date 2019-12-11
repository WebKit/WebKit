description("This test makes sure that quickly opening the same database multiple times is successful without the backend ASSERTing or crashing.");

function log(msg)
{
    document.getElementById("logger").innerHTML += msg + "";
}

function done()
{
    finishJSTest();
}

if (testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var dbname = "TestDB" + Date();

var req1 = indexedDB.open(dbname);
req1.onerror = function() {
    debug("First request unexpected error");
    done();
}

var req2 = indexedDB.open(dbname);
req2.onsuccess = function() {
    debug("Second request done");
    done();
}
req2.onerror = function() {
    debug("Second request unexpected error");
    done();
}
