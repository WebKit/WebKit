if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

var console = document.getElementById("console");

var messages = [];
var evalThunks = [];

function equal(actual, expected)
{
    var actualQueue = [actual];
    var expectedQueue = [expected];
    while (actualQueue.length && expectedQueue.length) {
        var actual = actualQueue.shift();
        var expected = expectedQueue.shift();

        if (actual === expected)
            continue;
        if (typeof actual !== typeof expected)
            return false;
        if ((actual instanceof Date) || (expected instanceof Date)) {
            if ((actual instanceof Date) && (expected instanceof Date) && actual.getTime() == expected.getTime())
                continue;
            return false;
        }
        if ((actual instanceof Number) || (expected instanceof Number)) {
            if ((actual instanceof Number) && (expected instanceof Number) && (expected.valueOf() == actual.valueOf()))
                continue;
            return false;
        }
        if ((actual instanceof Boolean) || (expected instanceof Boolean)) {
            if ((actual instanceof Boolean) && (expected instanceof Boolean) && (expected.valueOf() == actual.valueOf()))
                continue;
            return false;
        }
        if ((actual instanceof String) || (expected instanceof String)) {
            if ((actual instanceof String) && (expected instanceof String) && (expected.valueOf() == actual.valueOf()))
                continue;
            return false;
        }
        if (Array.isArray(actual) || Array.isArray(expected)) {
            if (!Array.isArray(actual) || !Array.isArray(expected))
                return false;
            if (actual.length != expected.length)
                return false;
            for (var i = 0; i < actual.length; i++) {
                if ((i in actual) ^ (i in expected))
                    return false;
                actualQueue.push(actual[i]);
                expectedQueue.push(expected[i]);
            }
            continue;
        }
        if (actual.constructor !== expected.constructor)
            return false;
        try {
            var keys = Object.keys(actual);
        } catch(e) {
            return false;
        }
        try {
        if (!equal(keys, Object.keys(expected)))
            return false;
        } catch(e) {
            return false;
        }
        for (var i = 0; i < keys.length; i++) {
            actualQueue.push(actual[keys[i]]);
            expectedQueue.push(expected[keys[i]]);
        }
    }

    return true;
}

function safeToString(o) {
    if (o instanceof Date)
        return o.toISOString();
    if (typeof o !== "object" || !o)
        return o;
    try {
        return o.toString();
    } catch (e) {
        return Object.prototype.toString.call(o) + "(default toString threw "+e+")";
    }
}

function shouldBe(actual, expected)
{
    var actualValue = eval(actual);
    var expectedValue = eval(expected);
    if (equal(actualValue, expectedValue))
        console.innerHTML += "PASS: " + actual + " is " + safeToString(expectedValue) + " of type " + typeof actualValue + "<br>";
    else
        console.innerHTML += "FAIL: " + actual + " is " + actualValue + " should be " + expectedValue + " of type " + typeof expectedValue + "<br>";
}

function shouldBeIdentical(actual, expected)
{
    var actualValue = eval(actual);
    var expectedValue = eval(expected);
    if (actualValue === expectedValue)
        console.innerHTML += "PASS: " + actual + " is === to " + expected + "<br>";
    else
        console.innerHTML += "FAIL: " + actual + " is not === to " + expected + "<br>";
}

function doPass(msg) {
    console.innerHTML += "PASS: " + msg + "<br>";
}

function doFail(msg) {
    console.innerHTML += "FAIL: " + msg + "<br>";
}

window.doPassFail = function(result, msg) {
    console.innerHTML += (result ? "PASS: " : "FAIL: ") + msg + "<br>";
}

function onmessage(evt) {
    eventData = evt.data;
    if (evt.data !== evt.data)
        console.innerHTML += "MessageEvent.data does not produce the same value on multiple queries. " + evt.data + ", " + evt.data + "<br>";
    var message = messages.shift();
    switch (message) {
    case "cyclicObject":
        shouldBeIdentical("eventData", "eventData.self");
        break;
    case "cyclicArray":
        shouldBeIdentical("eventData", "eventData[0]");
        break;
    case "objectGraph":
        shouldBeIdentical("eventData.graph1", "eventData.graph2");
        break;
    case "arrayGraph":
        shouldBeIdentical("eventData[0]", "eventData[1]");
        break;
    case "evalThunk":
        var thunk = evalThunks.shift();
        try {
            thunk(eventData);
        } catch(e) {
            doFail("threw exception: " + e);
            throw e;
        }
        break;
    default:
        shouldBe("eventData", message);
    }

    if (safeToString(evt.data) == 'done' && window.testRunner)
        testRunner.notifyDone();
}

window.addEventListener('message', onmessage, false);

function ConstructorWithPrototype(s) {
    this.field = s;
}

ConstructorWithPrototype.prototype = {
    protoProperty: 2010
};

window.tryPostMessage = function(message, shouldThrow, expected, expectedExceptionOrEvalThunk) {
    if (expected == "evalThunk") {
      var evalThunk = expectedExceptionOrEvalThunk;
    } else {
      var expectedException = expectedExceptionOrEvalThunk;
    }
    try {
        var value = eval(message);
        postMessage(value, "*");
        if (shouldThrow)
            console.innerHTML += "FAIL: 'postMessage("+message+")' should throw but didn't<br>";
        messages.push(expected || message);
        if (expected == "evalThunk") {
            evalThunks.push(evalThunk);
        }
    } catch(e) {
        if (shouldThrow) {
            if (expectedException === undefined || expectedException == e.code || expectedException == e) {
                console.innerHTML += "PASS: 'postMessage("+message+")' threw " + e + "<br>";
            } else {
                console.innerHTML += "FAIL: 'postMessage("+message+")' threw " + e + ", was expecting " + expectedException + "<br>";
            }
        } else
            console.innerHTML += "FAIL: 'postMessage("+message+")' should not throw but threw " + e + "<br>";
    }
}
