let foo = Object

function test() {
    return foo();
}
noInline(test);

for (i = 0; i < 100000; i++)
    test();
