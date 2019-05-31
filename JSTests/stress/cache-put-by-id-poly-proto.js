//@ runNoLLInt

function Foo() { }

let x = 0;

Object.defineProperty(Foo.prototype, 'y', {
    set(_) {
        if (x++ === 9) {
            Object.defineProperty(Foo.prototype, 'y', {
                value: 13,
                writable: true,
            });
            if (typeof $vm !== 'undefined')
                $vm.flattenDictionaryObject(Foo.prototype);
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
