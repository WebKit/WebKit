description(
'Tests to ensure that activations are built correctly in the face of duplicate parameter names and do not cause crashes.'
);

var s = {};

function eatRegisters(param)
{
    if (param > 10)
        return;
    eatRegisters(param + 1);
}
function test1(a, b, b, b, b, b, b) {
    return function() {
        return a[0];
    }
}

var test1Closure = test1(["success"]);

var extra = test1("success");
eatRegisters(0);
for (var i = 0; i < 1000000; ++i) {
    s = [{}];
}

shouldBe('test1Closure()', '"success"');

function test2(a, a, a, a, a, a, b) {
    return function() {
        return b[0];
    }
}

var test2Closure = test2("success", "success", "success", "success", "success", "success", ["success"]);
extra =  test2("success", "success", "success", "success", "success", "success", ["success"]);

eatRegisters(0);
for (var i = 0; i < 1000000; ++i) {
    s = [{}];
}

shouldBe('test2Closure()', '"success"');

var successfullyParsed = true;
