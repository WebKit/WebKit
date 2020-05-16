//@ skip if $memoryLimited
var hugeString = "x";
for (i = 0; i < 25; ++i) {
    hugeString += hugeString;
}

var exception;
var weird = 'Â';
try {
    var f = new Function(hugeString, hugeString, hugeString, hugeString, hugeString, hugeString, hugeString,
      hugeString, hugeString, hugeString, hugeString, hugeString, hugeString, hugeString,
      hugeString, hugeString, hugeString, hugeString, hugeString, hugeString, hugeString,
      () => 42,
      "return 42;");
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAIL";
