//@ skip

function testFunction(a, b)
{
    "use strict"
    result |= 0;
    return a * 1000 + b * 100 + arguments[2] * 10 + arguments.length ^ (result & 1024);
}

var arrayArguments = [2, 3, 4]
var result = 0;
for (var i = 0; i < 50000; i++)
    result += testFunction(...arrayArguments);

for (var i = 0; i < 50000; i++)
    result += testFunction(...[1, 2, result, 4]);

function test2() {
    for (var i = 0; i < 50000; i++)
        result += testFunction(...arguments);
}

test2(1,2,3,4)


function test3() {
    aliasedArguments = arguments;
    for (var i = 0; i < 50000; i++)
        result += testFunction(...aliasedArguments);
}

test3(1,2,result,4)

if (result != 43424761715344)
    throw "Result was " + result + " expected 43424761715344";
