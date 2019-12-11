if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's KeyRange.");

function incrementedKey(key)
{
    if (typeof key == "number")
        return key + 0.1;
    if (typeof key == "string") {
        if (key.length < 2)
            testFailed("String key is too short: " + key);
        if (key.charAt(key.length - 1) != "'")
            testFailed("Completely unexpected string key: " + key);
        
        return key.substring(0, key.length - 1) + key.charAt(key.length - 2) + "'";
    }
    
    testFailed("Incrementing unexpected key type: " + typeof key);
}

function decrementedKey(key)
{
    if (typeof key == "number")
        return key - 0.1;
    if (typeof key == "string") {
        if (key.length < 3)
            testFailed("String key is too short: " + key);
        if (key.charAt(key.length - 1) != "'")
            testFailed("Completely unexpected string key: " + key);
        
        return key.substring(0, key.length - 2) + "'";
    }
    
    testFailed("Decrementing unexpected key type: " + typeof key);
}

function checkSingleKeyRange(value)
{
    keyRange = evalAndLog("IDBKeyRange.only(" + value + ")");
    shouldBe("keyRange.lower", "" + value);
    shouldBe("keyRange.upper", "" + value);
    shouldBeFalse("keyRange.lowerOpen");
    shouldBeFalse("keyRange.upperOpen");
    shouldBeFalse("keyRange.includes(" + incrementedKey(value) + ")");
    shouldBeFalse("keyRange.includes(" + decrementedKey(value) + ")");
    shouldBeTrue("keyRange.includes(" + value + ")");
}

function checkLowerBoundKeyRange(value, open)
{
    keyRange = evalAndLog("IDBKeyRange.lowerBound(" + value + "," + open + ")");
    shouldBe("keyRange.lower", "" + value);
    if (open === undefined)
        open = false;
    shouldBe("keyRange.lowerOpen", "" + open);
    shouldBeUndefined("keyRange.upper");
    shouldBeTrue("keyRange.upperOpen");

    if (open)
        shouldBeFalse("keyRange.includes(" + value + ")");
    else
        shouldBeTrue("keyRange.includes(" + value + ")");

    shouldBeTrue("keyRange.includes(" + incrementedKey(value) + ")");
    shouldBeFalse("keyRange.includes(" + decrementedKey(value) + ")");
}

function checkUpperBoundKeyRange(value, open)
{
    keyRange = evalAndLog("IDBKeyRange.upperBound(" + value + "," + open + ")");
    shouldBe("keyRange.upper", "" + value);
    if (open === undefined)
        open = false;
    shouldBe("keyRange.upperOpen", "" + open);
    shouldBeUndefined("keyRange.lower");
    shouldBeTrue("keyRange.lowerOpen");

    if (open)
        shouldBeFalse("keyRange.includes(" + value + ")");
    else
        shouldBeTrue("keyRange.includes(" + value + ")");

    shouldBeFalse("keyRange.includes(" + incrementedKey(value) + ")");
    shouldBeTrue("keyRange.includes(" + decrementedKey(value) + ")");
}

function checkBoundKeyRange(lower, upper, lowerOpen, upperOpen)
{
    keyRange = evalAndLog("IDBKeyRange.bound(" + lower + "," + upper + ", " + lowerOpen + ", " + upperOpen + ")");
    shouldBe("keyRange.lower", "" + lower);
    shouldBe("keyRange.upper", "" + upper);
    if (lowerOpen === undefined)
        lowerOpen = false;
    if (upperOpen === undefined)
        upperOpen = false;
    shouldBe("keyRange.lowerOpen", "" + lowerOpen);
    shouldBe("keyRange.upperOpen", "" + upperOpen);
    
    if (lowerOpen)
        shouldBeFalse("keyRange.includes(" + lower + ")");
    else
        shouldBeTrue("keyRange.includes(" + lower + ")");

    if (upperOpen)
        shouldBeFalse("keyRange.includes(" + upper + ")");
    else
        shouldBeTrue("keyRange.includes(" + upper + ")");
}

function test()
{
    removeVendorPrefixes();
    shouldBeFalse("'lower' in IDBKeyRange");
    shouldBeFalse("'upper' in IDBKeyRange");
    shouldBeFalse("'lowerOpen' in IDBKeyRange");
    shouldBeFalse("'upperOpen' in IDBKeyRange");
    shouldBeFalse("'includes' in IDBKeyRange");
    shouldBeTrue("'only' in IDBKeyRange");
    shouldBeTrue("'lowerBound' in IDBKeyRange");
    shouldBeTrue("'upperBound' in IDBKeyRange");
    shouldBeTrue("'bound' in IDBKeyRange");

    debug("");

    var instance = evalAndLog("instance = IDBKeyRange.only(1)");
    shouldBeTrue("'lower' in instance");
    shouldBeTrue("'upper' in instance");
    shouldBeTrue("'lowerOpen' in instance");
    shouldBeTrue("'upperOpen' in instance");
    shouldBeTrue("'includes' in instance");
    shouldBeFalse("'only' in instance");
    shouldBeFalse("'lowerBound' in instance");
    shouldBeFalse("'upperBound' in instance");
    shouldBeFalse("'bound' in instance");

    debug("");

    checkSingleKeyRange(1);
    checkSingleKeyRange(3.14);
    checkSingleKeyRange("'a'");

    checkLowerBoundKeyRange(10, true);
    checkLowerBoundKeyRange(11, false);
    checkLowerBoundKeyRange(12);
    checkLowerBoundKeyRange(10.1, true);
    checkLowerBoundKeyRange(11.2, false);
    checkLowerBoundKeyRange(12.3);
    checkLowerBoundKeyRange("'aa'", true);
    checkLowerBoundKeyRange("'ab'", false);
    checkLowerBoundKeyRange("'ac'");

    checkUpperBoundKeyRange(20, true);
    checkUpperBoundKeyRange(21, false);
    checkUpperBoundKeyRange(22);
    checkUpperBoundKeyRange(20.2, true);
    checkUpperBoundKeyRange(21.3, false);
    checkUpperBoundKeyRange(22.4);
    checkUpperBoundKeyRange("'ba'", true);
    checkUpperBoundKeyRange("'bb'", false);
    checkUpperBoundKeyRange("'bc'");

    checkBoundKeyRange(30, 40);
    checkBoundKeyRange(31, 41, false, false);
    checkBoundKeyRange(32, 42, false, true);
    checkBoundKeyRange(33, 43, true, false);
    checkBoundKeyRange(34, 44, true, true);

    checkBoundKeyRange(30.1, 40.2);
    checkBoundKeyRange(31.3, 41.4, false, false);
    checkBoundKeyRange(32.5, 42.6, false, true);
    checkBoundKeyRange(33.7, 43.8, true, false);
    checkBoundKeyRange(34.9, 44.0, true, true);

    checkBoundKeyRange("'aaa'", "'aba'", false, false);
    checkBoundKeyRange("'aab'", "'abb'");
    checkBoundKeyRange("'aac'", "'abc'", false, false);
    checkBoundKeyRange("'aad'", "'abd'", false, true);
    checkBoundKeyRange("'aae'", "'abe'", true, false);
    checkBoundKeyRange("'aaf'", "'abf'", true, true);

    debug("Passing an invalid key into only({})");
    evalAndExpectException("IDBKeyRange.only({})", "0", "'DataError'");

    debug("Passing an invalid key into upperBound({})");
    evalAndExpectException("IDBKeyRange.upperBound({})", "0", "'DataError'");

    debug("Passing an invalid key into lowerBound({})");
    evalAndExpectException("IDBKeyRange.lowerBound({})", "0", "'DataError'");

    debug("Passing an invalid key into bound(null, {})");
    evalAndExpectException("IDBKeyRange.bound(null, {})", "0", "'DataError'");

    debug("Passing an invalid key into bound({},null)");
    evalAndExpectException("IDBKeyRange.bound({}, null)", "0", "'DataError'");

    debug("Passing an invalid key into bound({}, {})");
    evalAndExpectException("IDBKeyRange.bound({}, {})", "0", "'DataError'");

    debug("Lower key greater than higher key, bound(4, 3)");
    evalAndExpectException("IDBKeyRange.bound(4, 3)", "0", "'DataError'");

    debug("Equal keys, either of the bounds is open, bound(4, 4, true, false)");
    evalAndExpectException("IDBKeyRange.bound(4, 4, true, false)", "0", "'DataError'");

    debug("Equal keys, either of the bounds is open, bound(4, 4, false, true)");
    evalAndExpectException("IDBKeyRange.bound(4, 4, false, true)", "0", "'DataError'");

    debug("Equal keys, either of the bounds is open, bound(4, 4, true, true)");
    evalAndExpectException("IDBKeyRange.bound(4, 4, true, true)", "0", "'DataError'");

    debug("Equal keys, none of the bounds is open, bound(4, 4, false, false)");
    IDBKeyRange.bound(4, 4, false, false);

    debug("Passing an invalid key in to IDBKeyRange.includes({})");
    eval("invalidKeyKeyRange = IDBKeyRange.only('a')");
    evalAndExpectException("invalidKeyKeyRange.includes({})", "0", "'DataError'");
}

test();

finishJSTest();
