var object = {};
for (var i = 0; i < 1e3; ++i) {
    object[Symbol(i + 'prop')] = i;
}

function test(object)
{
    return Object.getOwnPropertySymbols(object);
}
noInline(test);

for (var i = 0; i < 1e3; ++i)
    test(object);
