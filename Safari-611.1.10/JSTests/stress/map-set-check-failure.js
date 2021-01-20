function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var func = Map.prototype.set;
function target(map)
{
    return func.call(map, 42, 42);
}
noInline(target);

for (var i = 0; i < 1e6; ++i) {
    var map = new Map();
    shouldBe(target(map), map);
    shouldBe(map.get(42), 42);
}
shouldThrow(() => {
    target(new Set());
}, `TypeError: Map operation called on non-Map object`);
