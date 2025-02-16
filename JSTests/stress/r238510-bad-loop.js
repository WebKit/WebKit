function foo() {
    return function () {
        eval();
    }
}
noInline(foo);

for (let i = 0; i < testLoopCount; ++i) {
    foo();    
}
