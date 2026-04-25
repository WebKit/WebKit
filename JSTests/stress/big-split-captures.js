"use strict";

var bigString = "xyz";
while (bigString.length < 200000)
    bigString = bigString + bigString;

if (bigString.length != 393216)
    throw "Error: bad string length: " + bigString.length;

var result = /(x)(y)(z)/[Symbol.split](bigString);

if (result.length != 524289)
    throw "Error: bad result array length: " + result.length;

if (result[0] != "")
    throw "Error: array does not start with an empty string.";

for (var i = 1; i < result.length; i += 4) {
    if (result[i + 0] != "x")
        throw "Error: array does not contain \"x\" at i = " + i + " + 0: " + result[i + 0];
    if (result[i + 1] != "y")
        throw "Error: array does not contain \"y\" at i = " + i + " + 1: " + result[i + 1];
    if (result[i + 2] != "z")
        throw "Error: array does not contain \"z\" at i = " + i + " + 2: " + result[i + 2];
    if (result[i + 3] != "")
        throw "Error: array does not contain \"\" at i = " + i + " + 3: " + result[i + 3];
}
