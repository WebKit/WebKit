description("Test basic dom storage .clear() functionality.");

function test(storageString)
{
    storage = eval(storageString);
    if (!storage) {
        testFailed(storageString + " DOES NOT exist");
        return;
    }

    debug("Testing " + storageString);

    evalAndLog("storage.clear()");
    shouldBe("storage.length", "0");

    evalAndLog("storage['FOO'] = 'MyFOO'");
    evalAndLog("storage['BAR'] = 'MyBar'");
    shouldBe("storage.length", "2");
    shouldBeEqualToString("storage['FOO']", "MyFOO");
    shouldBeEqualToString("storage['BAR']", "MyBar");

    evalAndLog("storage.clear()");
    shouldBe("storage.length", "0");
    shouldBe("storage['FOO']", "undefined");  // FIXME: Wait...shouldn't this be null?
    shouldBe("storage['BAR']", "undefined");  // ditto
}

test("sessionStorage");
debug("");
debug("");
test("localStorage");

window.successfullyParsed = true;
isSuccessfullyParsed();
