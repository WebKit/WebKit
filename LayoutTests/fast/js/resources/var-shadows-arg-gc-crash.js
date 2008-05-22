description(
'Tests to ensure that activations mark their values correctly in the face of duplicate parameter names and does not crash.'
);

var s = {};

function eatRegisters(param)
{
    if (param > 10)
        return;
    eatRegisters(param + 1);
}

function test(a, c) {
    var b = ["success"], a, c;
    return function() {
        return b[0];
    }
}

var testClosure = test();

var extra = test();
eatRegisters(0);
for (var i = 0; i < 1000000; ++i) {
    s = [{}];
}

shouldBe('testClosure()', '"success"');

var successfullyParsed = true;
