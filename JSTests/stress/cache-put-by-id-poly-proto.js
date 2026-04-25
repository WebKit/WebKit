//@ runNoLLInt

function Foo() { }

let x = 0;

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
    set(_) {
        if (x++ === 9) {
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

function createBar() {
    class Bar extends Foo {
        constructor() {
            super();
            this._y = 0;
        }
    }
    return new Bar();
};

for (let i = 0; i < 16; i++) {
    let bar = createBar();
    bar.y = 42;
}
