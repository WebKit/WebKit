description("Test that changing documentURI has no effects on the url passed into storage events.");

function test(storageString, callback)
{
    window.completionCallback = callback;
    window.storage = eval(storageString);
    if (!storage) {
        testFailed(storageString + " DOES NOT exist");
        return;
    }

    debug("Testing " + storageString);

    storageEventList = new Array();
    evalAndLog("storage.clear()");
    shouldBe("storage.length", "0");

    runAfterNStorageEvents(step1, 0);
}

function step1()
{
    debug("Reset storage event list");
    evalAndLog("storageEventList = new Array()");
    evalAndLog("storage.foo = '123'");

    runAfterNStorageEvents(step2, 1);
}

function step2()
{
    shouldBe("storageEventList.length", "1");
    debug("Saving url");
    window.lastURL = storageEventList[0].url;

    evalAndLog("document.documentURI = 'abc'");
    shouldBeEqualToString("document.documentURI", "abc");
    evalAndLog("storage.foo = 'xyz'");

    runAfterNStorageEvents(step3, 2);
}

function step3()
{
    shouldBe("storageEventList.length", "2");
    shouldBeTrue(String(window.lastURL == storageEventList[1].url));

    completionCallback();
}

testStorages(test);

var successfullyParsed = true;
