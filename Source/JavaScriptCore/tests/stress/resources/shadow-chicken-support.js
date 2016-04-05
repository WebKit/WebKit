"use strict";

function describeFunction(f)
{
    var name;
    try {
        name = f.name;
    } catch (e) {}
    if (!name)
        name = "<" + describe(f) + ">";
    return name;
}

function describeArray(array) {
    var result = "[";
    for (var i = 0; i < array.length; ++i) {
        if (i)
            result += ", ";
        result += describeFunction(array[i]);
    }
    return result + "]";
}

function compareStacks(stack, array) {
    if (stack.length != array.length)
        throw new Error("Bad stack length: " + describeArray(stack) + " (expected " + describeArray(array) + ")");
    for (var i = 0; i < stack.length; ++i) {
        if (stack[i] != array[i])
            throw new Error("Bad stack at i = " + i + ": " + describeArray(stack) + " (expected " + describeArray(array) + ")");
    }
}

function expectStack(array) {
    var stack = shadowChickenFunctionsOnStack();
    if (verbose)
        print("stack = " + describeArray(stack));
    var myTop = stack.pop();
    if (myTop != stackTop)
        throw new Error("Bad stack top: " + myTop);
    var myBottom = stack.shift();
    if (myBottom != shadowChickenFunctionsOnStack)
        throw new Error("Bad stack bottom: " + myBottom);
    myBottom = stack.shift();
    if (myBottom != expectStack)
        throw new Error("Bad stack next-to-bottom: " + myBottom);
    compareStacks(stack, array);
}

var initialShadow;
var stackTop;

function initialize()
{
    initialShadow = shadowChickenFunctionsOnStack();
    if (initialShadow.length != 3)
        throw new Error("bad initial shadow length: " + initialShadow.length);
    if (initialShadow[0] != shadowChickenFunctionsOnStack)
        throw new Error("bad top of stack: " + describeFunction(initialShadow[0]));
    if (initialShadow[1] != initialize)
        throw new Error("bad middle of stack: " + describeFunction(initialShadow[1]));
    stackTop = initialShadow[2];
    
    expectStack([initialize]);
}

