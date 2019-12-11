let flag = true;
function foo() {
    if (flag)
        return 20;
    return {};
}
noInline(foo);

let state = 0;
function e() {
    if ((++state) % 25 === 0)
        throw new Error();
}
noInline(e);

function baz() { }
noInline(baz);

function bar() {
    let x = foo();
    try {
        e();
        baz(++x);
    } catch(e) {
        baz(++x);
    } finally {
        baz(x);
    }
}
noInline(bar);

for (let i = 0; i < 2000; ++i) {
    bar();
}

flag = false;
for (let i = 0; i < 1000; ++i)
    bar();
