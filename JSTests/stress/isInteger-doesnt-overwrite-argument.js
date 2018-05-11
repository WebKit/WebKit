// This test shouldn't throw.

function testIsInteger(arg)
{
    var x = Number.isInteger(arg);
    return arg;
}

for (var i = 0; i < 100000; i++) {
    var r = testIsInteger(13.37);
    if (r === false)
        throw "Wrong value returned from function calling Number.isInteger";
}
