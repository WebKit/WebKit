function test(prototype)
{
    return Object.create(prototype);
}

var prototype = {foo: 42};
for (var i = 0; i < 1e5; ++i) {
    test(prototype);
}
