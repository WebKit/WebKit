//@ skip if $memoryLimited

function foo(str, count) {
    while (str.length < count) {
        try {
           str += str;
        } catch (e) {}
    }
    return str.substring();
}
var x = foo("1", 1 << 20);
var y = foo("$1", 1 << 16);

var exception;
try {
    var __v_6623 = x.replace(/(.+)/g, y);
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
