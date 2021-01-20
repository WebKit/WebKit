"use strict";

var bigString = "xy";
while (bigString.length < 200000)
    bigString = bigString + bigString;

if (bigString.length != 262144)
    throw "Error: bad string length: " + bigString.length;

var result = /x/[Symbol.split](bigString);

if (result.length != 131073)
    throw "Error: bad result array length: " + result.length;

if (result[0] != "")
    throw "Error: array does not start with an empty string.";

for (var i = 1; i < result.length; ++i) {
    if (result[i] != "y")
        throw "Error: array does not contain \"y\" at i = " + i + ": " + result[i];
}
