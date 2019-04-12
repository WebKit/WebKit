const x = createGlobalObject();

function foo(resolve) {
    0 instanceof resolve;
}

noInline(x.Promise);

for (let i = 0; i < 1000; i++) {
    new x.Promise(foo);
}
