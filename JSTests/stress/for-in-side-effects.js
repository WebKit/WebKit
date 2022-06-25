// Regression test for bug 179212

var p = { "a": {} };

var flag = 0;
var data = [];
var copy = [];

var z = new Proxy({}, {
    getPrototypeOf: function() {
        if (flag == 2) {
            data[0] = { "x": "I changed" };
        }

        if (flag == 1) {
            flag = 2;
        }

        return {"a": 1, "b": 2}
    }
});

p.__proto__ = z;

function reset()
{
    flag = 0;
    data = [1.1, 2.2, 3.3];
    copy = [];
}

function runTest(func)
{
    reset();

    for (var i = 0; i < 0x10000; i++)
        func();

    flag = 1;
    func();

    if (copy[0].x != "I changed")
        throw "Expected updated value for copy[0]";
}

function testWithoutFTL()
{
    function f()
    {
        data[0] = 2.2;
        for(var d in p) {
            copy[0] = data[0];
            copy[1] = data[1];
            copy[2] = data[2];
        }
    }

    noFTL(f);

    runTest(f);
}

function testWithFTL()
{
    function f()
    {
        data[0] = 2.2;
        for(var d in p) {
            copy[0] = data[0];
            copy[1] = data[1];
            copy[2] = data[2];
        }
    }

    runTest(f);
}

testWithoutFTL();
testWithFTL();
