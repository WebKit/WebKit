//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function trial()
{
    var buffer = new ArrayBuffer(10000000);
    var int8View = new Int8Array(buffer);
    for (var i = 0; i < 300000; ++i) {
        var start = Date.now();
        var result = Object.getOwnPropertySymbols(int8View);
        var delta = Date.now() - start;
        if (delta > 1000)
            throw new Error(`time consuming (${delta}ms)`);
    }
}

trial();
