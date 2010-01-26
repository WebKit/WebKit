description("Test that changing documentURI has no effects on the uri passed into storage events.");

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
    evalAndLog("storage.foo = '123'");

    runAfterStorageEvents(step2);
}

function step2()
{
    shouldBe("storageEventList.length", "1");
    debug("Saving URI");
    window.lastURI = storageEventList[0].uri;

    evalAndLog("document.documentURI = 'abc'");
    shouldBeEqualToString("document.documentURI", "abc");
    evalAndLog("storage.foo = 'xyz'");

    runAfterStorageEvents(step3);
}

function step3()
{
    shouldBe("storageEventList.length", "2");
    shouldBeTrue(String(window.lastURI == storageEventList[1].uri));

    completionCallback();
}

testStorages(test);

var successfullyParsed = true;
