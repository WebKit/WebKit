function inner(object, key1)
{
    var { [key1]: a, [3]: b, ...rest } = object;
    return rest.d + a + b;
}

function outer(object)
{
    if (inner(object, 'a') !== 3)
        throw new Error("Bad assertion!");
}
noInline(outer);

var object = { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0, g: 0, h: 0, i: 0, j: 0, [3]: 1 };

for (var i = 0; i < 5e3; i++)
    outer(object);
