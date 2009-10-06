description("Test whether we can add >5mb (the default quota) of data to DOM Storage");

function runTest(storageString)
{
    storage = eval(storageString);
    if (!storage) {
        testFailed(storageString + " DOES NOT exist");
        return;
    }

    debug("Testing " + storageString);

    evalAndLog("storage.clear()");
    shouldBe("storage.length", "0");

    debug("Creating 'data' which contains 64K of data");
    data = "X";
    for (var i=0; i<16; i++)
        data += data;
    shouldBe("data.length", "65536");

    debug("Putting 'data' into 40 " + storageString + " buckets.");
    for (var i=0; i<40; i++)
        storage[i] = data;

    debug("Putting 'data' into another bucket.h");
    try {
        storage[40] = data;
        testPassed("Insertion worked.");
    } catch (e) {
        testFailed("Exception: " + e);
    }

    window.successfullyParsed = true;
}
