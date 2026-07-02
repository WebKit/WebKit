description(
'Tests to ensure that activations are built correctly in the face of duplicate parameter names and do not cause crashes.'
);

function gc()
{
    if (this.GCController)
        GCController.collect();
    else
        for (var i = 0; i < 10000; ++i) // Allocate a sufficient number of objects to force a GC.
            ({});
}

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
gc();

shouldBe('test1Closure()', '"success"');

function test2(a, a, a, a, a, a, b) {
    return function() {
        return b[0];
    }
}

var test2Closure = test2("success", "success", "success", "success", "success", "success", ["success"]);
extra =  test2("success", "success", "success", "success", "success", "success", ["success"]);

eatRegisters(0);
gc();

shouldBe('test2Closure()', '"success"');
