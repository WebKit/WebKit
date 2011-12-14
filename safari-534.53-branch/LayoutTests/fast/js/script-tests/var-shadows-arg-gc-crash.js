description(
'Tests to ensure that activations mark their values correctly in the face of duplicate parameter names and does not crash.'
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

function test(a, c) {
    var b = ["success"], a, c;
    return function() {
        return b[0];
    }
}

var testClosure = test();

var extra = test();
eatRegisters(0);
gc();

shouldBe('testClosure()', '"success"');

var successfullyParsed = true;
