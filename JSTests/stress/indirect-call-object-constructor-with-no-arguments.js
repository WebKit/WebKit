let foo = Object

function test() {
    return foo();
}
noInline(test);

for (i = 0; i < testLoopCount; i++)
    test();
