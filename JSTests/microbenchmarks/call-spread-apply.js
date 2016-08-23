
Function.prototype.a = Function.prototype.apply;

function testFunction(a, b)
{
    "use strict"
    return this * 10000 + a * 1000 + b * 100 + arguments[2] * 10 + arguments.length;
}

var arrayArguments = [1, [2, 3, 4]]

for (var i = 0; i < 10000; i++) {
    var result1 = testFunction.apply(...arrayArguments);
    var result2 = testFunction.a(...arrayArguments);
    if (result1 != result2) 
        throw "Call with spread array failed at iteration " + i + ": " + result1 + " vs " + result2;
}

for (var i = 0; i < 10000; i++) {
    var result1 = testFunction.apply(...[1, [2, 3, 4]]);
    var result2 = testFunction.a(...[1, [2, 3, 4]]);
    if (result1 != result2) 
        throw "Call with spread array failed at iteration " + i + ": " + result1 + " vs " + result2;
}

function test2() {
    for (var i = 0; i < 10000; i++) {
        var result1 = testFunction.apply(...arguments);
        var result2 = testFunction.a(...arguments);
        if (result1 != result2)
           throw "Call with spread arguments failed at iteration " + i + ": " + result1 + " vs " + result2;
    }
}

test2(1,[2,3,4])


function test3() {
    aliasedArguments = arguments;
    for (var i = 0; i < 10000; i++) {
        var result1 = testFunction.apply(...aliasedArguments);
        var result2 = testFunction.a(...aliasedArguments);
        if (result1 != result2)
           throw "Call with spread arguments failed at iteration " + i + ": " + result1 + " vs " + result2;
    }
}

test3(1,[2,3,4])