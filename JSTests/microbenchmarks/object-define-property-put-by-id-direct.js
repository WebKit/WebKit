// Exercises the Object.defineProperty(fresh, key, { value, writable:true,
// enumerable:true, configurable:true }) pattern. With a fresh object and
// all-default attributes, the DFG/FTL ConstantFolding lowers the
// ObjectDefineProperty → ObjectDefinePropertyFromFields →
// DefineDataProperty → PutByIdDirect chain, so this should run through
// the PutById inline cache on hot paths.

function bench() {
    var sum = 0;
    for (var i = 0; i < 1000000; i++) {
        var obj = {};
        Object.defineProperty(obj, "prop", { value: i, writable: true, enumerable: true, configurable: true });
        sum += obj.prop;
    }
    return sum;
}

var result = bench();
if (result !== 499999500000)
    throw new Error("Bad result: " + result);
