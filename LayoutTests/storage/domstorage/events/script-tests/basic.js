description("This is a test to make sure DOM Storage mutations fire StorageEvents that are caught by the event listener set via window.onstorage.");

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

    runAfterStorageEvents(step1);
}

function step1()
{
    debug("Reset storage event list");
    evalAndLog("storageEventList = new Array()");
    evalAndLog("storage.setItem('FOO', 'BAR')");

    runAfterStorageEvents(step2);
}

function step2()
{
    shouldBe("storageEventList.length", "1");
    shouldBeEqualToString("storageEventList[0].key", "FOO");
    shouldBeNull("storageEventList[0].oldValue");
    shouldBeEqualToString("storageEventList[0].newValue", "BAR");
    evalAndLog("storage.setItem('FU', 'BAR')");
    evalAndLog("storage.setItem('a', '1')");
    evalAndLog("storage.setItem('b', '2')");
    evalAndLog("storage.setItem('b', '3')");

    runAfterStorageEvents(step3);
}

function step3()
{
    shouldBe("storageEventList.length", "5");
    shouldBeEqualToString("storageEventList[1].key", "FU");
    shouldBeNull("storageEventList[1].oldValue");
    shouldBeEqualToString("storageEventList[1].newValue", "BAR");
    shouldBeEqualToString("storageEventList[2].key", "a");
    shouldBeNull("storageEventList[2].oldValue");
    shouldBeEqualToString("storageEventList[2].newValue", "1");
    shouldBeEqualToString("storageEventList[3].key", "b");
    shouldBeNull("storageEventList[3].oldValue");
    shouldBeEqualToString("storageEventList[3].newValue", "2");
    shouldBeEqualToString("storageEventList[4].key", "b");
    shouldBeEqualToString("storageEventList[4].oldValue", "2");
    shouldBeEqualToString("storageEventList[4].newValue", "3");
    evalAndLog("storage.removeItem('FOO')");

    runAfterStorageEvents(step4);
}

function step4()
{
    shouldBe("storageEventList.length", "6");
    shouldBeEqualToString("storageEventList[5].key", "FOO");
    shouldBeEqualToString("storageEventList[5].oldValue", "BAR");
    shouldBeNull("storageEventList[5].newValue");
    evalAndLog("storage.removeItem('FU')");

    runAfterStorageEvents(step5);
}

function step5()
{
    shouldBe("storageEventList.length", "7");
    shouldBeEqualToString("storageEventList[6].key", "FU");
    shouldBeEqualToString("storageEventList[6].oldValue", "BAR");
    shouldBeNull("storageEventList[6].newValue");
    evalAndLog("storage.clear()");
 
    runAfterStorageEvents(step6);
}

function step6()
{
    shouldBe("storageEventList.length", "8");
    shouldBeNull("storageEventList[7].key");
    shouldBeNull("storageEventList[7].oldValue");
    shouldBeNull("storageEventList[7].newValue");
 
    completionCallback();
}

testStorages(test);

var successfullyParsed = true;
