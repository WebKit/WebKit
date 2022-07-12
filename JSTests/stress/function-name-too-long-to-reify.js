//@ skip if $memoryLimited
//@ runDefault
var prop = "".padEnd(2 ** 31 - 1, "a");

function shouldBe(actual, expected) {
    if (String(actual) !== expected)
        throw new Error('Actual Value:' + actual + 'Expected Value:' + expected);
    return true;
}

function shouldThrow(func, expectedError) {
    var errorThrown = false;
    var ActualError = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        ActualError = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (!shouldBe(ActualError, expectedError))
        throw new Error(`bad error: ${String(ActualError)}`);
}

function classSetter() {
    class A {
        set [prop](_) {
        }
    }
}

function objectSetter() {
    let obj = {
        set [prop](_) {
        }
    };
}

function classGetter() {
    class A {
        get [prop]() {
        }
    }
}

function objectGetter() {
    let obj = {
        get [prop]() {
        }
    };
}

setterExpectedError = "RangeError: Out of memory: Setter name is too long";
getterExpectedError = "RangeError: Out of memory: Getter name is too long";

shouldThrow(classSetter, setterExpectedError);
shouldThrow(classSetter, setterExpectedError); // Make sure it should still throw.
shouldThrow(classGetter, getterExpectedError);
shouldThrow(classGetter, getterExpectedError); // Make sure it should still throw.

shouldThrow(objectSetter, setterExpectedError);
shouldThrow(objectSetter, setterExpectedError); // Make sure it should still throw.
shouldThrow(objectGetter, getterExpectedError);
shouldThrow(objectGetter, getterExpectedError); // Make sure it should still throw.
