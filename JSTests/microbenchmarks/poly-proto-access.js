//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(b) { if (!b) throw new Error("Bad"); }
noInline(assert);

function foo() {
    class C {
        constructor()
        {
            this.x = 20;
        }

        get bar()
        {
            assert(this.x === 20);
            assert(this.foo === undefined || this.foo === 42);
            return 45;
        }
    }
    return new C;
}

foo();
let a = [];
for (let i = 0; i < 15; ++i)
    a.push(foo());

function bar(o) {
    assert(o.foo === undefined || o.foo === 42);
    assert(o.bar === 45);
}
noInline(bar);

let start = Date.now();
for (let i = 0; i < 100000; ++i) {
    if (i === 5000) {
        for (let arr of a)
            arr.__proto__.foo = 42;
    }
    for (let j = 0; j < a.length; ++j) {
        bar(a[j]);
    }
}
if (false)
    print(Date.now() - start);
