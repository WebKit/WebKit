function foo(x) {
    delete x[x];
}
noInline(foo);

for (let i = 0; i < 20000; i++)
    foo({});

