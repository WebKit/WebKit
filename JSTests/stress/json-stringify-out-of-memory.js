//@ skip if $memoryLimited or $buildType == "debug"
//@ runDefault

var a = [];
var str = "a";

for (var i = 0; i < 8; i++) {
  str += str;
  str += String.fromCharCode(i, i) + str.trimLeft();
}
for (var i = 0; i < 10000; i++)
  a.push(str);

var exception;
try {
    json1 = JSON.stringify(a);
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAIL";

