var DB_TEST_SUFFIX = "_dom";

function openDatabaseWithSuffix(name, version, description, size, callback)
{
    if (arguments.length > 4) {
        return openDatabase(name + DB_TEST_SUFFIX, version, description, size, callback);
    } else {
        return openDatabase(name + DB_TEST_SUFFIX, version, description, size);
    }
}

function log(message)
{
    document.getElementById("console").innerText += message + "\n";
}

function setLocationHash(hash) {
    location.hash = hash;
}

function setupAndRunTest()
{
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }
    document.getElementById("console").innerText = "";
    runTest();
}
