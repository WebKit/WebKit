const length = 100;

var target = $vm.createAlwaysSlowPutContiguousObjectWithOverrides(length);
for (var i = 0; i < length; i++)
    target[i] = {index: i};

var reversed = Array.prototype.reverse.call(target);
if (reversed.putByIndexCalls !== length * 2)
    throw new Error(`Bad value: ${reversed.putByIndexCalls}!`);
