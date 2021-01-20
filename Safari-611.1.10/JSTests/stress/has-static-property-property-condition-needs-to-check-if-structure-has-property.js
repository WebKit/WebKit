let o = {};
let p = $vm.createStaticCustomValue();
o.__proto__ = p;

function foo(o) {
    return o.testStaticValue;
}
noInline(foo);

foo(o);
foo(o);
foo(o);
foo(o);
p.testStaticValue = 42;

for (let i = 0; i < 100; ++i) {
    foo(o);
}
