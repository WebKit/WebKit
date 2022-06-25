function foo(s) {
    return /.*/.exec(s);
}

noInline(foo);

for (var i = 0; i < 10000; ++i)
    foo("foo bar");

RegExp.input = "blah";

var didFinish = false;
try {
    foo({toString: function() {
        throw "error";
    }});
    didFinish = true;
} catch (e) {
    if (e != "error")
        throw "Error: bad exception at end: " + e;
    if (RegExp.input != "blah")
        throw "Error: bad value of input: " + RegExp.input;
}

if (didFinish)
    throw "Error: did not throw exception.";
