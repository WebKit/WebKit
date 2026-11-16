//@ runNoLLInt

function Foo() { }

Foo.prototype.x = 0;

function makePrototypeDict() {
    for (let i=0; i<100; ++i) {
        Foo.prototype.a = i
        Foo.prototype.b = i
        delete Foo.prototype.a
        delete Foo.prototype.b
    }
}
noInline(makePrototypeDict)

Object.defineProperty(Foo.prototype, 'y', {
    set(x) {
        if (Foo.prototype.x++ === 1) {
            delete Foo.prototype.x;
            if (typeof $vm !== 'undefined') {
                makePrototypeDict()
                $vm.flattenDictionaryObject(Foo.prototype);
            }
        }
    }
});

let foo = new Foo();
while (typeof foo.x === 'number')
    foo.y = 42;
