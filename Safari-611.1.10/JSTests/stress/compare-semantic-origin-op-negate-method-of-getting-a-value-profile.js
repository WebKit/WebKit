let flag = false;
function a() { return flag ? {} : 10; }
noInline(a);
function b() { return 10.2; }
noInline(b);

function foo(x) {
    let r = -(x ? a() : b());
    return r;
}
noInline(foo);

for (let i = 0; i < 100000; ++i)
    foo(!!(i%2));

flag = true;
for (let i = 0; i < 100000; ++i)
    foo(!!(i%2));
