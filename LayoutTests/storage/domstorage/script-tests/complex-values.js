description("Test some corner case DOM Storage values.");

eventCounter = 0;
function handleStorageEvent() {
    eventCounter++;
}

function testKeyValue(key, value)
{
    keyString = "storage['" + key + "']";
    shouldBeEqualToString("typeof " + keyString, "string");
    shouldBeEqualToString(keyString, value);

    keyString = "storage." + key;
    shouldBeEqualToString("typeof " + keyString, "string");
    shouldBeEqualToString(keyString, value);

    keyString = "storage.getItem('" + key + "')";
    shouldBeEqualToString("typeof " + keyString, "string");
    shouldBeEqualToString(keyString, value);
}

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
    shouldBeEqualToString("typeof storage['foo']", "undefined");
    shouldBeUndefined("storage['foo']");
    shouldBeEqualToString("typeof storage.foo", "undefined");
    shouldBeUndefined("storage.foo");
    shouldBeEqualToString("typeof storage.getItem('foo')", "object");
    shouldBeNull("storage.getItem('foo')");

    debug("");
    evalAndLog("storage.foo1 = null");
    testKeyValue("foo1", "null");
    evalAndLog("storage['foo2'] = null");
    testKeyValue("foo2", "null");
    evalAndLog("storage.setItem('foo3', null)");
    testKeyValue("foo3", "null");

    debug("");
    evalAndLog("storage.foo4 = undefined");
    testKeyValue("foo4", "undefined");
    evalAndLog("storage['foo5'] = undefined");
    testKeyValue("foo5", "undefined");
    evalAndLog("storage.setItem('foo6', undefined)");
    testKeyValue("foo6", "undefined");

    debug("");
    evalAndLog("storage.foo7 = 2");
    testKeyValue("foo7", "2");
    evalAndLog("storage['foo8'] = 2");
    testKeyValue("foo8", "2");
    evalAndLog("storage.setItem('foo9', 2)");
    testKeyValue("foo9", "2");

    debug("");
    k = String.fromCharCode(255425) + String.fromCharCode(255) + String.fromCharCode(2554252321) + String.fromCharCode(0) + 'hello';
    evalAndLog("storage.foo10 = k");
    testKeyValue("foo10", k);
    evalAndLog("storage['foo11'] = k");
    testKeyValue("foo11", k);
    evalAndLog("storage.setItem('foo12', k)");
    testKeyValue("foo12", k);

    debug("");
    debug("Verify storage events are case sensitive");
    evalAndLog("storage.foo = 'test'");
    debug("Setting event listener");
    window.addEventListener("storage", handleStorageEvent, false);
    shouldBe("eventCounter", "0");
    evalAndLog("storage.foo = 'test'");
    shouldBe("eventCounter", "0");
    evalAndLog("storage.foo = 'TEST'");
    shouldBe("eventCounter", "1");

    // Reset the counter for next tests (if any).
    window.removeEventListener("storage", handleStorageEvent, false);
    eventCounter = 0;
}

test("sessionStorage");
debug("");
debug("");
test("localStorage");

window.successfullyParsed = true;
isSuccessfullyParsed();
