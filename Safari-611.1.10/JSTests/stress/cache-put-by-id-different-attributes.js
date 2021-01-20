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
        if (Foo.prototype.x++ === 9) {
            Object.defineProperty(Foo.prototype, 'y', {
                value: 13,
                writable: true,
            });
            if (typeof $vm !== 'undefined') {
                makePrototypeDict()
                $vm.flattenDictionaryObject(Foo.prototype);
            }
        }
    },
    configurable: true,
});

let foo = new Foo();
for (let i = 0; i < 11; i++)
    foo.y = 42;
