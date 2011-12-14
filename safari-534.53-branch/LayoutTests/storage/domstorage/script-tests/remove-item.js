description("Test .removeItem within DOM Storage.");

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

    debug("");
    shouldBeUndefined("storage.foo1");
    evalAndLog("storage.foo1 = 'bar'");
    shouldBeEqualToString("storage.foo1", "bar");
    evalAndLog("storage.removeItem('foo1')");
    shouldBeUndefined("storage.foo1");
    evalAndLog("storage.removeItem('foo1')");
    shouldBeUndefined("storage.foo1");

    debug("");
    shouldBeUndefined("storage['foo2']");
    evalAndLog("storage['foo2'] = 'bar'");
    shouldBeEqualToString("storage['foo2']", "bar");
    evalAndLog("storage.removeItem('foo2')");
    shouldBeUndefined("storage['foo2']");
    evalAndLog("storage.removeItem('foo2')");
    shouldBeUndefined("storage['foo2']");

    debug("");
    shouldBeNull("storage.getItem('foo3')");
    evalAndLog("storage.setItem('foo3', 'bar')");
    shouldBeEqualToString("storage.getItem('foo3')", "bar");
    evalAndLog("storage.removeItem('foo3')");
    shouldBeNull("storage.getItem('foo3')");
    evalAndLog("storage.removeItem('foo3')");
    shouldBeNull("storage.getItem('foo3')");
}

test("sessionStorage");
debug("");
debug("");
test("localStorage");

window.successfullyParsed = true;
isSuccessfullyParsed();
