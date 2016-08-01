var a = [ "String", false, 42 ];
var count = 0;

function getX(fromDFG) {
    if (fromDFG)
        return 42;
    return false;
}

noInline(getX);

function foo(index) {
    var result = false;
    var x = getX(DFGTrue());

    x * 2;

    var y = a[index % a.length];
    result = y === x;
    count += 1;
    return result;
}

noInline(foo);

var loopCount = 10000;

function bar() {
    var result;

    for (var i = 0; i < loopCount - 1; i++)
        result = foo(i)

    result = foo(0);

    return result;
}

var result = bar();
if (result != false)
    throw "Error: bad result expected false: " + result;
if (count != loopCount)
    throw "Error: bad count, expected: " + loopCount + ", got: " + count;
