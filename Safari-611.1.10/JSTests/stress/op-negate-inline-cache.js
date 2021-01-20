"use strict"


function opaqueIdentity(arg) {
    return arg;
}
noInline(opaqueIdentity)
function negateWithDoubleSub(arg) {
    // Implement integer negate as a double sub operation.
    return opaqueIdentity(6.4 - arg) - 6.4;
}
noInline(negateWithDoubleSub)

function opaqueNonZeroIntegerNegate(arg)
{
    return -arg;
}
noInline(opaqueNonZeroIntegerNegate);

function testNonZeroInteger()
{
    for (let i = 1; i < 1e4; ++i) {
        if (opaqueNonZeroIntegerNegate(i) !== negateWithDoubleSub(i)) {
            throw "Failed testNonZeroInteger() at i = " + i;
        }
    }
}
testNonZeroInteger();


function opaqueDoubleNegate(arg)
{
    return -arg;
}
noInline(opaqueDoubleNegate);

function testDouble()
{
    for (let i = 0; i < 1e4; ++i) {
        if ((opaqueDoubleNegate(i + 0.5)) + 0.5 + i !== 0) {
            throw "Failed testDouble() at i = " + i;
        }
    }
}
testDouble();


function opaqueObjectNegate(arg)
{
    return -arg;
}
noInline(opaqueObjectNegate);

function testObject()
{
    for (let i = 0; i < 1e4; ++i) {
        if ((opaqueObjectNegate({ valueOf: ()=>{ return i + 0.5 }})) + 0.5 + i !== 0) {
            throw "Failed testObject() at i = " + i;
        }
    }
}
testObject();


function opaqueIntegerAndDoubleNegate(arg)
{
    return -arg;
}
noInline(opaqueIntegerAndDoubleNegate);

function testIntegerAndDouble()
{
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueIntegerAndDoubleNegate(i)) + i !== 0) {
            throw "Failed testIntegerAndDouble() on integers at i = " + i;
        }
        if ((opaqueIntegerAndDoubleNegate(i + 0.5)) + 0.5 + i !== 0) {
            throw "Failed testIntegerAndDouble() on double at i = " + i;
        }
    }
}
testIntegerAndDouble();


function opaqueIntegerThenDoubleNegate(arg)
{
    return -arg;
}
noInline(opaqueIntegerThenDoubleNegate);

function testIntegerThenDouble()
{
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueIntegerThenDoubleNegate(i)) + i !== 0) {
            throw "Failed testIntegerThenDouble() on integers at i = " + i;
        }
    }
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueIntegerThenDoubleNegate(i + 0.5)) + 0.5 + i !== 0) {
            throw "Failed testIntegerThenDouble() on double at i = " + i;
        }
    }
}
testIntegerThenDouble();


function opaqueDoubleThenIntegerNegate(arg)
{
    return -arg;
}
noInline(opaqueDoubleThenIntegerNegate);

function testDoubleThenInteger()
{
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueDoubleThenIntegerNegate(i + 0.5)) + 0.5 + i !== 0) {
            throw "Failed testDoubleThenInteger() on double at i = " + i;
        }
    }
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueDoubleThenIntegerNegate(i)) + i !== 0) {
            throw "Failed testDoubleThenInteger() on integers at i = " + i;
        }
    }
}
testDoubleThenInteger();


function opaqueIntegerAndObjectNegate(arg)
{
    return -arg;
}
noInline(opaqueIntegerAndObjectNegate);

function testIntegerAndObject()
{
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueIntegerAndObjectNegate(i)) + i !== 0) {
            throw "Failed testIntegerAndObject() on integers at i = " + i;
        }
        if ((opaqueIntegerAndObjectNegate({ valueOf: ()=>{ return i + 0.5 }})) + 0.5 + i !== 0) {
            throw "Failed testIntegerAndObject() on double at i = " + i;
        }
    }
}
testIntegerAndObject();


function opaqueDoubleAndObjectNegate(arg)
{
    return -arg;
}
noInline(opaqueDoubleAndObjectNegate);

function testDoubleAndObject()
{
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueDoubleAndObjectNegate(i + 0.5)) + i + 0.5 !== 0) {
            throw "Failed testDoubleAndObject() on integers at i = " + i;
        }
        if ((opaqueDoubleAndObjectNegate({ valueOf: ()=>{ return i }})) + i !== 0) {
            throw "Failed testDoubleAndObject() on double at i = " + i;
        }
    }
}
testDoubleAndObject();


function opaqueIntegerThenObjectNegate(arg)
{
    return -arg;
}
noInline(opaqueIntegerThenObjectNegate);

function testIntegerThenObject()
{
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueIntegerThenObjectNegate(i)) + i !== 0) {
            throw "Failed testIntegerThenObject() on integers at i = " + i;
        }
    }
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueIntegerThenObjectNegate({ valueOf: ()=>{ return i + 0.5 }})) + 0.5 + i !== 0) {
            throw "Failed testIntegerThenObject() on double at i = " + i;
        }
    }
}
testIntegerThenObject();


function opaqueDoubleThenObjectNegate(arg)
{
    return -arg;
}
noInline(opaqueDoubleThenObjectNegate);

function testDoubleThenObject()
{
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueDoubleThenObjectNegate(i + 0.5)) + i + 0.5 !== 0) {
            throw "Failed testDoubleThenObject() on integers at i = " + i;
        }
    }
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueDoubleThenObjectNegate(i + 0.5)) + i + 0.5 !== 0) {
            throw "Failed testDoubleThenObject() on integers at i = " + i;
        }
    }
}
testDoubleThenObject();


function opaqueIntegerAndDoubleAndObjectNegate(arg)
{
    return -arg;
}
noInline(opaqueIntegerAndDoubleAndObjectNegate);

function testIntegerAndDoubleAndObject()
{
    for (let i = 1; i < 1e4; ++i) {
        if ((opaqueIntegerAndDoubleAndObjectNegate(i)) + i !== 0) {
            throw "Failed testIntegerAndDoubleAndObject() on integers at i = " + i;
        }
        if ((opaqueIntegerAndDoubleAndObjectNegate(i + 0.5)) + i + 0.5 !== 0) {
            throw "Failed testIntegerAndDoubleAndObject() on integers at i = " + i;
        }
        if ((opaqueIntegerAndDoubleAndObjectNegate({ valueOf: ()=>{ return i }})) + i !== 0) {
            throw "Failed testIntegerAndDoubleAndObject() on double at i = " + i;
        }
    }
}
testIntegerAndDoubleAndObject();


function opaqueIntegerNegateOverflow(arg)
{
    return -arg;
}
noInline(opaqueIntegerNegateOverflow);

function testIntegerNegateOverflow()
{
    for (let i = 1; i < 1e4; ++i) {
        if (opaqueIntegerNegateOverflow(0x80000000|0) !== 2147483648) {
            throw "Failed opaqueIntegerNegateOverflow() at i = " + i;
        }
    }
}
testIntegerNegateOverflow();


function opaqueIntegerNegateZero(arg)
{
    return -arg;
}
noInline(opaqueIntegerNegateZero);

function testIntegerNegateZero()
{
    for (let i = 1; i < 1e4; ++i) {
        if (1 / opaqueIntegerNegateZero(0) !== -Infinity) {
            throw "Failed testIntegerNegateZero() at i = " + i;
        }
    }
}
testIntegerNegateZero();


function gatedNegate(selector, arg)
{
    if (selector === 0) {
        return -arg;
    }
    if (selector == 42) {
        return -arg;
    }
    return arg;
}
noInline(gatedNegate);

function testUnusedNegate()
{
    for (let i = 1; i < 1e2; ++i) {
        if (gatedNegate(Math.PI, i) !== i) {
            throw "Failed first phase of testUnusedNegate";
        }
    }
    for (let i = 1; i < 1e4; ++i) {
        if (gatedNegate(0, i) + i !== 0) {
            throw "Failed second phase of testUnusedNegate";
        }
    }
    for (let i = 1; i < 1e4; ++i) {
        if (gatedNegate(42, i + 0.5) + 0.5 + i !== 0) {
            throw "Failed third phase of testUnusedNegate";
        }
    }
}
testUnusedNegate();
