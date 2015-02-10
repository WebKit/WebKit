function foo() { return arguments.length; }

var o = {};
o[0] = 42;
var callCount = 0;
o.__defineGetter__("length", function() {
    callCount++;
    return 1;
});

function bar() {
    callCount = 0;
    var result = foo.apply(this, o);
    if (result != 1)
        throw "Error: bad result: " + result;
    if (callCount != 1)
        throw "Error: bad call count: " + callCount;
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 10000; ++i)
    bar();
