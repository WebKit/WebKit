description("Test dom storage with many different types of keys (as opposed to values)");

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

    debug("");
    shouldBeNull("storage.getItem('FOO')");
    evalAndLog("storage.setItem('FOO', 'BAR')");
    shouldBe("storage.length", "1");

    shouldBeEqualToString("storage.getItem('FOO')", "BAR");
    shouldBeNull("storage.getItem('foo')");
    shouldBeUndefined("storage.foo");
    shouldBeUndefined("storage['foo']");

    evalAndLog("storage.foo = 'x'");
    shouldBeEqualToString("storage.foo", "x");
    shouldBeEqualToString("storage['foo']", "x");
    shouldBeEqualToString("storage.getItem('foo')", "x");
    evalAndLog("storage['foo'] = 'y'");
    shouldBeEqualToString("storage.foo", "y");
    shouldBeEqualToString("storage['foo']", "y");
    shouldBeEqualToString("storage.getItem('foo')", "y");
    evalAndLog("storage.setItem('foo', 'z')");
    shouldBeEqualToString("storage.foo", "z");
    shouldBeEqualToString("storage['foo']", "z");
    shouldBeEqualToString("storage.getItem('foo')", "z");
    shouldBe("storage.length", "2");

    debug("");
    debug("Testing a null key");
    evalAndLog("storage.setItem(null, 'asdf')");
    shouldBeEqualToString("storage.getItem('null')", "asdf");
    shouldBeEqualToString("storage.getItem(null)", "asdf");
    shouldBeEqualToString("storage['null']", "asdf");
    shouldBeEqualToString("storage[null]", "asdf");
    shouldBe("storage.length", "3");

    evalAndLog("storage[null] = 1");
    shouldBeEqualToString("storage.getItem(null)", "1");
    evalAndLog("storage['null'] = 2");
    shouldBeEqualToString("storage.getItem(null)", "2");
    evalAndLog("storage.setItem('null', 3)");
    shouldBeEqualToString("storage.getItem(null)", "3");
    shouldBe("storage.length", "3");

    debug("");
    debug("Testing an undefined key");
    evalAndLog("storage[undefined] = 'xyz'");
    shouldBeEqualToString("storage.getItem('undefined')", "xyz");
    shouldBeEqualToString("storage.getItem(undefined)", "xyz");
    shouldBeEqualToString("storage['undefined']", "xyz");
    shouldBeEqualToString("storage[undefined]", "xyz");
    shouldBe("storage.length", "4");

    evalAndLog("storage['undefined'] = 4");
    shouldBeEqualToString("storage.getItem(undefined)", "4");
    evalAndLog("storage.setItem(undefined, 5)");
    shouldBeEqualToString("storage.getItem(undefined)", "5");
    evalAndLog("storage.setItem('undefined', 6)");
    shouldBeEqualToString("storage.getItem(undefined)", "6");
    shouldBe("storage.length", "4");

    debug("");
    debug("Testing a numeric key");
    evalAndLog("storage['2'] = 'ppp'");
    shouldBeEqualToString("storage.getItem('2')", "ppp");
    shouldBeEqualToString("storage.getItem(2)", "ppp");
    shouldBeEqualToString("storage['2']", "ppp");
    shouldBeEqualToString("storage[2]", "ppp");
    shouldBe("storage.length", "5");

    evalAndLog("storage[2] = 7");
    shouldBeEqualToString("storage.getItem(2)", "7");
    evalAndLog("storage.setItem(2, 8)");
    shouldBeEqualToString("storage.getItem(2)", "8");
    evalAndLog("storage.setItem('2', 9)");
    shouldBeEqualToString("storage.getItem(2)", "9");
    shouldBe("storage.length", "5");

    debug("");
    debug("Setting a non-ascii string to foo");
    k = String.fromCharCode(255425) + String.fromCharCode(255) + String.fromCharCode(2554252321) + String.fromCharCode(0) + 'hello';
    evalAndLog("storage[k] = 'hello'");
    shouldBeEqualToString("storage.getItem(k)", "hello");
    shouldBeEqualToString("storage[k]", "hello");
    shouldBe("storage.length", "6");

    debug("");
    debug("Testing case differences");
    evalAndLog("storage.foo1 = 'lower1'");
    evalAndLog("storage.FOO1 = 'UPPER1'");
    evalAndLog("storage['foo2'] = 'lower2'");
    evalAndLog("storage['FOO2'] = 'UPPER2'");
    evalAndLog("storage.setItem('foo3', 'lower3')");
    evalAndLog("storage.setItem('FOO3', 'UPPER3')");
    shouldBeEqualToString("storage.foo1", "lower1");
    shouldBeEqualToString("storage.FOO1", "UPPER1");
    shouldBeEqualToString("storage['foo2']", "lower2");
    shouldBeEqualToString("storage['FOO2']", "UPPER2");
    shouldBeEqualToString("storage.getItem('foo3')", "lower3");
    shouldBeEqualToString("storage.getItem('FOO3')", "UPPER3");
    shouldBe("storage.length", "12");
    

    debug("");
    debug("Testing overriding length");
    shouldBe("storage.length", "12");
    shouldBe("storage['length']", "12");
    shouldBeNull("storage.getItem('length')");

    evalAndLog("storage.length = 0");
    shouldBe("storage.length", "12");
    shouldBe("storage['length']", "12");
    shouldBeNull("storage.getItem('length')");

    evalAndLog("storage['length'] = 0");
    shouldBe("storage.length", "12");
    shouldBe("storage['length']", "12");
    shouldBeNull("storage.getItem('length')");

    evalAndLog("storage.setItem('length', 0)");
    shouldBe("storage.length", "13");
    shouldBe("storage['length']", "13");
    shouldBeEqualToString("storage.getItem('length')", "0");

    evalAndLog("storage.removeItem('length')");
    shouldBe("storage.length", "12");
    shouldBe("storage['length']", "12");
    shouldBeNull("storage.getItem('length')");

    evalAndLog("storage.setItem('length', 0)");
    shouldBe("storage.length", "13");

    window.successfullyParsed = true;
}
