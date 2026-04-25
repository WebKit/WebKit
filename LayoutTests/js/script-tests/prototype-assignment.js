//@ runFTLNoCJIT

// This test suite compares the behavior of setting the prototype on various values
// (using Object.setPrototypeOf(), obj.__proto__ assignment, and Reflect.setPrototypeOf())
// against what is specified in the ES spec.  The expected behavior specified according
// to the spec is expressed in expectationsForObjectSetPrototypeOf,
// expectationsForSetUnderscoreProto, and expectationsForReflectSetPrototypeOf.


var inBrowser = (typeof window != "undefined");

// Test configuration options:
var verbose = false;
var maxIterations = 1;
var throwOnEachError = true;

var testUndefined = true;
var testNull = true;
var testTrue = true;
var testFalse = true;
var testNumbers = true;
var testString = true;
var testSymbol = true;
var testObject = true;
var testGlobal = true;
var testWindowProtos = inBrowser;

//====================================================================================
// Error messages:

if (inBrowser) {
    let userAgent = navigator.userAgent;
    if (userAgent.match(/ Chrome\/[0-9]+/)) engine = "chrome";
    else if (userAgent.match(/ Firefox\/[0-9]+/)) engine = "default";
    else engine = "safari";
} else
    engine = "jsc";

// Set default error messages and then override with engine specific ones below.
DefaultTypeError = "TypeError: ";
CannotSetPrototypeOfImmutablePrototypeObject = DefaultTypeError;
CannotSetPrototypeOfThisObject = DefaultTypeError;
CannotSetPrototypeOfUndefinedOrNull = DefaultTypeError;
CannotSetPrototypeOfNonObject = DefaultTypeError;
PrototypeValueCanOnlyBeAnObjectOrNull = DefaultTypeError;
ObjectProtoCalledOnNullOrUndefinedError = DefaultTypeError;
ReflectSetPrototypeOfRequiresTheFirstArgumentBeAnObject = DefaultTypeError;
ReflectSetPrototypeOfRequiresTheSecondArgumentBeAnObjectOrNull = DefaultTypeError;

if (engine == "jsc" || engine === "safari") {
    CannotSetPrototypeOfImmutablePrototypeObject = "TypeError: Cannot set prototype of immutable prototype object";
    CannotSetPrototypeOfThisObject = "TypeError: Cannot set prototype of this object";
    CannotSetPrototypeOfUndefinedOrNull = "TypeError: Cannot set prototype of undefined or null";
    CannotSetPrototypeOfNonObject = "TypeError: Cannot set prototype of non-object";
    PrototypeValueCanOnlyBeAnObjectOrNull = "TypeError: Prototype value can only be an object or null";
    ObjectProtoCalledOnNullOrUndefinedError = "TypeError: Object.prototype.__proto__ called on null or undefined";
    ReflectSetPrototypeOfRequiresTheFirstArgumentBeAnObject = "TypeError: Reflect.setPrototypeOf requires the first argument be an object";
    ReflectSetPrototypeOfRequiresTheSecondArgumentBeAnObjectOrNull = "TypeError: Reflect.setPrototypeOf requires the second argument be either an object or null";
} else if (engine === "chrome") {
    CannotSetPrototypeOfImmutablePrototypeObject = "TypeError: Immutable prototype object ";
}

//====================================================================================
// Utility functions:

if (inBrowser)
    print = console.log;

function reportError(errorMessage) {
    if (throwOnEachError)
        throw errorMessage;
    else
        print(errorMessage);
}

function shouldEqual(testID, resultType, actual, expected) {
    if (actual != expected)
        reportError("ERROR in " + resultType
            + ": expect " + stringify(expected) + ", actual " + stringify(actual)
            + " in test: " + testID.signature + " on iteration " + testID.iteration);
}

function shouldThrow(testID, resultType, actual, expected) {
    let actualStr = "" + actual;
    if (!actualStr.startsWith(expected))
        reportError("ERROR in " + resultType
            + ": expect " + expected + ", actual " + actual
            + " in test: " + testID.signature + " on iteration " + testID.iteration);
}

function stringify(value) {
    if (typeof value == "string") return '"' + value + '"';
    if (typeof value == "symbol") return value.toString();

    if (value === origGlobalProto) return "origGlobalProto"; 
    if (value === origObjectProto) return "origObjectProto"; 
    if (value === newObjectProto) return "newObjectProto";
    if (value === proxyObject) return "proxyObject";

    if (value === null) return "null";
    if (typeof value == "object") return "object"; 
    return "" + value;
}

function makeTestID(index, iteration, targetName, protoToSet, protoSetter, expected) {
    let testID = {};
    testID.signature = "[" + index + "] "
        + protoSetter.actionName + "|"
        + targetName + "|"
        + stringify(protoToSet) + "|"
        + stringify(expected.result) + "|"
        + stringify(expected.proto) + "|"
        + stringify(expected.exception);
    testID.iteration = iteration;
    return testID;
}

//====================================================================================
// Functions to express the expectations of the ES specification:

function doInternalSetPrototypeOf(result, target, origProto, newProto) {
    if (!target.setPrototypeOf) {
        result.success = true;
        result.exception = undefined;
        return;
    }
    target.setPrototypeOf(result, origProto, newProto);
}

// 9.1.2.1 OrdinarySetPrototypeOf ( O, V )
// https://tc39.github.io/ecma262/#sec-ordinarysetprototypeof
function ordinarySetPrototypeOf(result, currentProto, newProto) {
    // 9.1.2.1-4 If SameValue(V, current) is true, return true.
    if (newProto === currentProto) {
        result.success = true;
        return;
    }
    // 9.1.2.1-5 [extensibility check not tested here]
    // 9.1.2.1-8 [cycle check not tested here]
    result.success = true;
}

// 9.4.7.2 SetImmutablePrototype ( O, V )
// https://tc39.github.io/ecma262/#sec-set-immutable-prototype
function setImmutablePrototype(result, currentProto, newProto) {
    if (newProto === currentProto) {
        result.success = true;
        return;
    }
    result.success = false;
    result.exception = CannotSetPrototypeOfImmutablePrototypeObject;
}


var count = 0;
function initSetterExpectation(target, newProto) {
    var targetValue = target.value();
    var origProto = undefined;
    if (targetValue != null && targetValue != undefined)
        origProto = targetValue.__proto__; // Default to old proto.

    var expected = {};
    expected.targetValue = targetValue;
    expected.origProto = origProto;
    expected.exception = undefined;
    expected.proto = origProto;
    expected.result = undefined;

    return expected;
}

// 19.1.2.21 Object.setPrototypeOf ( O, proto )
// https://tc39.github.io/ecma262/#sec-object.setprototypeof
function objectSetPrototypeOf(target, newProto) {
    let expected = initSetterExpectation(target, newProto);
    var targetValue = expected.targetValue;
    var origProto = expected.origProto;

    function throwIfNoExceptionPending(e) {
        if (!expected.exception)
            expected.exception = e;
    }

    // 19.1.2.21-1 Let O be ? RequireObjectCoercible(O).
    if (targetValue == undefined || targetValue == null)
        throwIfNoExceptionPending(CannotSetPrototypeOfUndefinedOrNull);

    // 19.1.2.21-2 If Type(proto) is neither Object nor Null, throw a TypeError exception.
    if (typeof newProto != "object")
        throwIfNoExceptionPending(PrototypeValueCanOnlyBeAnObjectOrNull);

    // 19.1.2.21-3 If Type(O) is not Object, return O.
    else if (typeof targetValue != "object")
        expected.result = targetValue;

    // 19.1.2.21-4 Let status be ? O.[[SetPrototypeOf]](proto).
    else {
        // 19.1.2.21-5 If status is false, throw a TypeError exception.
        let result = {};
        doInternalSetPrototypeOf(result, target, origProto, newProto);
        if (result.success)
            expected.proto = newProto;
        else
            throwIfNoExceptionPending(result.exception);

        // 19.1.2.21-6 Return O.
        expected.result = targetValue;
    }

    return expected;
}
objectSetPrototypeOf.action = (obj, newProto) => Object.setPrototypeOf(obj, newProto);
objectSetPrototypeOf.actionName = "Object.setPrototypeOf";


// B.2.2.1.2 set Object.prototype.__proto__
// https://tc39.github.io/ecma262/#sec-set-object.prototype.__proto__
function setUnderscoreProto(target, newProto) {
    let expected = initSetterExpectation(target, newProto);
    var targetValue = expected.targetValue;
    var origProto = expected.origProto;

    function throwIfNoExceptionPending(e) {
        if (!expected.exception)
            expected.exception = e;
    }

    // B.2.2.1.2-1 Let O be ? RequireObjectCoercible(this value).
    if (targetValue == undefined || targetValue == null)
        throwIfNoExceptionPending(DefaultTypeError);

    // B.2.2.1.2-2 If Type(proto) is neither Object nor Null, return undefined.
    if (typeof newProto != "object")
        expected.result = undefined;
    
    // B.2.2.1.2-3 If Type(O) is not Object, return undefined.
    else if (typeof targetValue != "object")
        expected.result = undefined;

    // B.2.2.1.2-4 Let status be ? O.[[SetPrototypeOf]](proto).
    else {
        // B.2.2.1.2-5 If status is false, throw a TypeError exception.
        let result = {};
        doInternalSetPrototypeOf(result, target, origProto, newProto);
        if (result.success)
            expected.proto = newProto;
        else
            throwIfNoExceptionPending(result.exception);

        // B.2.2.1.2-6 Return undefined.
        expected.result = undefined;
    }

    // Override the result to be the newProto value because the statement obj.__proto__ = value
    // will produce the rhs value, not the result of the obj.__proto__ setter.
    expected.result = newProto;
    return expected;    
}
setUnderscoreProto.action = (obj, newProto) => (obj.__proto__ = newProto);
setUnderscoreProto.actionName = "obj.__proto__";


// 26.1.13 Reflect.setPrototypeOf ( target, proto )
// https://tc39.github.io/ecma262/#sec-reflect.setprototypeof
// function expectationsForReflectSetPrototypeOf(target, newProto, targetExpectation) {
function reflectSetPrototypeOf(target, newProto) {
    let expected = initSetterExpectation(target, newProto);
    var targetValue = expected.targetValue;
    var origProto = expected.origProto;

    function throwIfNoExceptionPending(e) {
        if (!expected.exception)
            expected.exception = e;
    }

    // 26.1.13-1 If Type(target) is not Object, throw a TypeError exception.
    if (targetValue === null || typeof targetValue != "object")
        throwIfNoExceptionPending(ReflectSetPrototypeOfRequiresTheFirstArgumentBeAnObject);

    // 26.1.13-2 If Type(proto) is not Object and proto is not null, throw a TypeError exception.
    if (typeof newProto != "object")
        throwIfNoExceptionPending(ReflectSetPrototypeOfRequiresTheSecondArgumentBeAnObjectOrNull);
    
    // 26.1.13-3 Return ? target.[[SetPrototypeOf]](proto).
    let result = {};
    doInternalSetPrototypeOf(result, target, origProto, newProto);
    expected.result = result.success;
    if (result.success)
        expected.proto = newProto;

    return expected;    
}
reflectSetPrototypeOf.action = (obj, newProto) => Reflect.setPrototypeOf(obj, newProto);
reflectSetPrototypeOf.actionName = "Reflect.setPrototypeOf";


//====================================================================================
// Test Data:

var global = this;
var origGlobalProto = global.__proto__;
var origObjectProto = {}.__proto__;
var proxyObject = new Proxy({ }, {
    setPrototypeOf(target, value) {
        throw "Thrown from proxy";
    }
});
var newObjectProto = { toString() { return "newObjectProto"; } };

var targets = [];

if (this.testUndefined) targets.push({ name: "undefined", value: () => undefined });
if (this.testNull)      targets.push({ name: "null", value: () => null });
if (this.testTrue)     targets.push({ name: "true", value: () => true });
if (this.testFalse)        targets.push({ name: "false", value: () => false });

if (this.testNumbers) {
    targets.push({ name: "0", value: () => 0 });
    targets.push({ name: "11", value: () => 11 });
    targets.push({ name: "123.456", value: () => 123.456 });
}

if (this.testString)    targets.push({ name: '"doh"', value: () => "doh" });
if (this.testSymbol)    targets.push({ name: "Symbol(doh)", value: () => Symbol("doh") });

if (this.testObject) {
    targets.push({
        name: "{}", 
        value: () => {},
        setPrototypeOf: ordinarySetPrototypeOf
    });
    targets.push({
        name: "{}.__proto__", 
        value: () => origObjectProto,
        setPrototypeOf: setImmutablePrototype
    });
}
if (this.testGlobal) {
    targets.push({
        name: inBrowser ? "window" : "global",
        value: () => global,
        setPrototypeOf: setImmutablePrototype
    });
}
if (this.testWindowProtos) {
    if (inBrowser) {
        let depth = 1;
        let proto = window.__proto__;
        while (proto) { 
            let name = "window";
            for (var i = 0; i < depth; i++)
                name += ".__proto__";

            let currentProto = proto;
            targets.push({
                name: name, 
                value: () => currentProto,
                setPrototypeOf: setImmutablePrototype
            });

            proto = proto.__proto__;
            depth++;
        }
    }
}


var newProtos = [
    undefined,
    null,
    true,
    false,
    0,
    11,
    123.456,
    "doh",
    Symbol("doh"),
    {},
    origObjectProto,
    origGlobalProto,
    newObjectProto
];

var protoSetters = [
    objectSetPrototypeOf,
    setUnderscoreProto,
    reflectSetPrototypeOf,
];


//====================================================================================
// Test driver functions:

function test(testID, targetValue, newProto, setterAction, expected) {
    let exception = undefined;
    let result = undefined;
    try {
        result = setterAction(targetValue, newProto);
    } catch (e) {
        exception = e;
    }
    shouldThrow(testID, "exception", exception, expected.exception);
    if (!expected.exception) {
        shouldEqual(testID, "__proto__", targetValue.__proto__, expected.proto);
        shouldEqual(testID, "result", result, expected.result);
    }
}

function runTests() {
    let testIndex = 0;
    for (protoSetter of protoSetters) {
        for (target of targets) {
            for (newProto of newProtos) {
                let currentTestIndex = testIndex++;
                for (var i = 0; i < maxIterations; i++) {
                    let expected = protoSetter(target, newProto);
                    let targetValue = expected.targetValue;

                    let testID = makeTestID(currentTestIndex, i, target.name, newProto, protoSetter, expected);
                    if (verbose && i == 0)
                        print("test: " + testID.signature);

                    test(testID, targetValue, newProto, protoSetter.action, expected);
                }
            }
        }
    }
}

runTests();
