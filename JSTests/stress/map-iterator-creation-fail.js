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

function test1()
{
    Map.prototype.values.call({});
}
noInline(test1);

for (var i = 0; i < 1e3; ++i) {
    shouldThrow(() => {
        test1();
    }, `TypeError: Map operation called on non-Map object`);
}

function test2()
{
    Map.prototype.keys.call({});
}
noInline(test2);

for (var i = 0; i < 1e3; ++i) {
    shouldThrow(() => {
        test2();
    }, `TypeError: Map operation called on non-Map object`);
}

function test3()
{
    Map.prototype.entries.call({});
}
noInline(test3);

for (var i = 0; i < 1e3; ++i) {
    shouldThrow(() => {
        test3();
    }, `TypeError: Map operation called on non-Map object`);
}
