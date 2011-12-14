// 15.4 Array Objects
// (c) 2001 Harri Porten <porten@kde.org>

description(
'This test checks for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=4147">4147: Array.toString() and toLocaleString() improvements from KDE KJS</a>.'
);

// backup
var backupNumberToString = Number.prototype.toString;
var backupNumberToLocaleString = Number.prototype.toLocaleString;
var backupRegExpToString = RegExp.prototype.toString;
var backupRegExpToLocaleString = RegExp.prototype.toLocaleString;

// change functions
Number.prototype.toString = function() { return "toString"; }
Number.prototype.toLocaleString = function() { return "toLocaleString"; }
RegExp.prototype.toString = function() { return "toString2"; }
RegExp.prototype.toLocaleString = function() { return "toLocaleString2"; }

// the tests
shouldBe("[1].toString()", "'1'");
shouldBe("[1].toLocaleString()", "'toLocaleString'");
Number.prototype.toLocaleString = "invalid";
shouldBe("[1].toLocaleString()", "'1'");
shouldBe("[/r/].toString()", "'toString2'");
shouldBe("[/r/].toLocaleString()", "'toLocaleString2'");
RegExp.prototype.toLocaleString = "invalid";
shouldBe("[/r/].toLocaleString()", "'toString2'");

var caught = false;
try {
[{ toString : 0 }].toString();
} catch (e) {
caught = true;
}
shouldBeTrue("caught");

// restore
Number.prototype.toString = backupNumberToString;
Number.prototype.toLocaleString = backupNumberToLocaleString;
RegExp.prototype.toString = backupRegExpToString;
RegExp.prototype.toLocaleString = backupRegExpToLocaleString;
var successfullyParsed = true;
