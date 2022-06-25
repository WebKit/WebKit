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

function test1(set)
{
    Set.prototype.values.call(set);
}
noInline(test1);

var set = new Set();
for (var i = 0; i < 1e6; ++i)
    test1(set);

shouldThrow(() => {
    test1({});
}, `TypeError: Set operation called on non-Set object`);

function test2(set)
{
    Set.prototype.keys.call(set);
}
noInline(test2);

for (var i = 0; i < 1e6; ++i)
    test2(set);

shouldThrow(() => {
    test2({});
}, `TypeError: Set operation called on non-Set object`);

function test3(set)
{
    Set.prototype.entries.call(set);
}
noInline(test3);

for (var i = 0; i < 1e3; ++i)
    test3(set);

shouldThrow(() => {
    test3({});
}, `TypeError: Set operation called on non-Set object`);
