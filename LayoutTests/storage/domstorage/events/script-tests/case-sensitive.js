description("Verify that storage events fire even when only the case of the value changes.");

function test(storageString, callback)
{
    window.completionCallback = callback;
    window.storage = eval(storageString);
    if (!storage) {
        testFailed(storageString + " DOES NOT exist");
        return;
    }

    debug("Testing " + storageString);

    evalAndLog("storage.clear()");
    shouldBe("storage.length", "0");

    debug("");
    debug("Verify storage events are case sensitive");
    evalAndLog("storage.foo = 'test'");

    runAfterStorageEvents(step1);
}

function step1()
{
    debug("Reset storage event list");
    evalAndLog("storageEventList = new Array()");
    evalAndLog("storage.foo = 'test'");

    runAfterStorageEvents(step2);
}

function step2()
{
    shouldBe("storageEventList.length", "0");
    evalAndLog("storage.foo = 'TEST'");

    runAfterStorageEvents(step3);
}

function step3()
{
    shouldBe("storageEventList.length", "1");

    completionCallback();
}

testStorages(test);

var successfullyParsed = true;
