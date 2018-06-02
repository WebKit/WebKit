function test(prototype)
{
    return Object.create(prototype);
}
noInline(test);

var prototype1 = {};
var prototype2 = [];
var prototype3 = new Date();
var prototype4 = { hello: 42 };
var prototype5 = new Map();
for (var i = 0; i < 1e5; ++i) {
    test(prototype1);
    test(prototype2);
    test(prototype3);
    test(prototype4);
    test(prototype5);
}
