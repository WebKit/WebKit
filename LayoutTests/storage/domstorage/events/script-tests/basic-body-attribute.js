description("This is a test to make sure DOM Storage mutations fire StorageEvents that are caught by the event listener specified as an attribute on the body.");

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

    evalAndLog("iframe.onload = step1");
    evalAndLog("iframe.src = 'resources/body-event-handler.html'");
}

function step1()
{
    debug("Reset storage event list");
    evalAndLog("storageEventList = new Array()");
    evalAndLog("storage.setItem('FOO', 'BAR')");

    runAfterNStorageEvents(step2, 1);
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

    runAfterNStorageEvents(step3, 5);
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

    runAfterNStorageEvents(step4, 6);
}

function step4()
{
    shouldBe("storageEventList.length", "6");
    shouldBeEqualToString("storageEventList[5].key", "FOO");
    shouldBeEqualToString("storageEventList[5].oldValue", "BAR");
    shouldBeNull("storageEventList[5].newValue");
    evalAndLog("storage.removeItem('FU')");

    runAfterNStorageEvents(step5, 7);
}

function step5()
{
    shouldBe("storageEventList.length", "7");
    shouldBeEqualToString("storageEventList[6].key", "FU");
    shouldBeEqualToString("storageEventList[6].oldValue", "BAR");
    shouldBeNull("storageEventList[6].newValue");
    evalAndLog("storage.clear()");
 
    runAfterNStorageEvents(step6, 8);
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
