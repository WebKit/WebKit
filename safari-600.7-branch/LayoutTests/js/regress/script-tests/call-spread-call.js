
Function.prototype.c = Function.prototype.call;

function testFunction(a, b)
{
    "use strict"
    return this * 10000 + a * 1000 + b * 100 + arguments[2] * 10 + arguments.length;
}

var arrayArguments = [1, 2, 3, 4]

for (var i = 0; i < 15000; i++) {
    var result1 = testFunction.call(...arrayArguments);
    var result2 = testFunction.c(...arrayArguments);
    if (result1 != result2) 
        throw "Call with spread array failed at iteration " + i + ": " + result1 + " vs " + result2;
}

function test2() {
    for (var i = 0; i < 15000; i++) {
        var result1 = testFunction.call(...arguments);
        var result2 = testFunction.c(...arguments);
        if (result1 != result2)
           throw "Call with spread arguments failed at iteration " + i + ": " + result1 + " vs " + result2;
    }
}

test2(1,2,3,4)


function test3() {
    aliasedArguments = arguments;
    for (var i = 0; i < 15000; i++) {
        var result1 = testFunction.call(...aliasedArguments);
        var result2 = testFunction.c(...aliasedArguments);
        if (result1 != result2)
           throw "Call with spread arguments failed at iteration " + i + ": " + result1 + " vs " + result2;
    }
}

test3(1,2,3,4)