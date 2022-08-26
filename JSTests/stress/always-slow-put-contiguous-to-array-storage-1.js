function assert(x) { if (!x) throw new Error(`Bad assertion: ${x}!`); }

function putByVal(target, index) { "use strict"; target[index] = {index}; }
noInline(putByVal);

const runs = 1e5;
const length = 100;

var target = $vm.createAlwaysSlowPutContiguousObjectWithOverrides(length);
for (var i = 0; i < length; i++)
    target[i] = {index: i};

(function() {
    for (var i = 0; i < runs; i++) {
        if (i === runs - 1)
            target[10000001] = 1;

        for (var j = 0; j < length; j++)
            putByVal(target, j);
    }

    assert(target.putByIndexCalls === length + (length * runs) + 1);
    assert($vm.indexingMode(target) === "NonArrayWithSlowPutArrayStorage");
    assert($vm.hasSparseModeArrayStorage(target));
})();
