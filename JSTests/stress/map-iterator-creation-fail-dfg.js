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

function test1(map)
{
    Map.prototype.values.call(map);
}
noInline(test1);

var map = new Map();
for (var i = 0; i < 1e6; ++i)
    test1(map);

shouldThrow(() => {
    test1({});
}, `TypeError: Map operation called on non-Map object`);

function test2(map)
{
    Map.prototype.keys.call(map);
}
noInline(test2);

for (var i = 0; i < 1e6; ++i)
    test2(map);

shouldThrow(() => {
    test2({});
}, `TypeError: Map operation called on non-Map object`);

function test3(map)
{
    Map.prototype.entries.call(map);
}
noInline(test3);

for (var i = 0; i < 1e3; ++i)
    test3(map);

shouldThrow(() => {
    test3({});
}, `TypeError: Map operation called on non-Map object`);
