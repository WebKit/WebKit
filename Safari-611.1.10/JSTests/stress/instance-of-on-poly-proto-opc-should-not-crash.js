function makePolyProtoObject() {
    function foo() {
        class C {
            constructor() {
                this._field = 42;
                this.hello = 33;
            }
        };
        return new C;
    }
    for (let i = 0; i < 15; ++i)
        foo();
    return foo();
}

function foo(o, c) {
    return o instanceof c;
}
noInline(foo);

class C { }

let o = makePolyProtoObject();
o.__proto__= new C;
let x = {__proto__: o};
for (let i = 0; i < 1000; ++i) {
    foo(x, C);
}
