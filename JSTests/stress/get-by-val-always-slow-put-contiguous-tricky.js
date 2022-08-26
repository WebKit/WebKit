function assert(x) { if (!x) throw new Error(`Bad assertion: ${x}!`); }

function getByVal(target, index) { return target[index]; }
noInline(getByVal);

const runs = 1e6;
const length = 100;

var jsArrayContiguous = new Array(length);
var alwaysSlowPutContiguous = $vm.createAlwaysSlowPutContiguousObjectWithOverrides(length);
for (var i = 0; i < length; i++)
    jsArrayContiguous[i] = alwaysSlowPutContiguous[i] = {index: i};

(function() {
    for (var i = 0; i < runs; i++) {
        for (var j = 0; j < length; j++)
            assert(getByVal(i === runs - 100 ? alwaysSlowPutContiguous : jsArrayContiguous, j).index === j);
    }
})();
