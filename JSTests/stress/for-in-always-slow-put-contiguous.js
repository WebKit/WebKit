function assert(x) { if (!x) throw new Error(`Bad assertion: ${x}!`); }

const runs = 1e5;
const length = 100;

var alwaysSlowPutContiguous = $vm.createAlwaysSlowPutContiguousObjectWithOverrides(length);
for (var i = 0; i < length; i++)
    alwaysSlowPutContiguous[i] = {index: i};

(function() {
    for (var i = 0; i < runs; i++) {
        for (var j in alwaysSlowPutContiguous)
            assert(alwaysSlowPutContiguous[j].index === +j);
    }
})();
