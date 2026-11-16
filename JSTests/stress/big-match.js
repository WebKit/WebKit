"use strict";

var bigString = "x";
while (bigString.length < 200000)
    bigString = bigString + bigString;

if (bigString.length != 262144)
    throw "Error: bad string length: " + bigString.length;

var result = /x/g[Symbol.match](bigString);

if (result.length != 262144)
    throw "Error: bad result array length: " + result.length;

for (var i = 0; i < result.length; ++i) {
    if (result[i] != "x")
        throw "Error: array does not contain \"x\" at i = " + i + ": " + result[i];
}


