//@ runDefault
var array = [];
for (var i = 0; i < 0x20000; ++i)
    array.push(i);

function nop()
{
}
noInline(nop);

function test(array) {
    return nop.apply([], array);
}
noInline(test);

for (var i = 0; i < 100; ++i)
    test(array);
