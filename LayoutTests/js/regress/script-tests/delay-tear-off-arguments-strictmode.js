function bar(x)
{
	"use strict";
    var result = 0;
    for (var i = 0; i < arguments.length; ++i)
        result += arguments[i];
    return result;
}

var result = 0;
for (var i = 0; i < 100000; ++i)
    result += bar(i, i + 1, i + 2, i + 3);

if (result != 20000400000)
    throw "Bad result: " + result;
