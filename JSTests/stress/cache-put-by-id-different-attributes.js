//@ runNoLLInt

function Foo() { }

Foo.prototype.x = 0;

Object.defineProperty(Foo.prototype, 'y', {
    set(x) {
        if (Foo.prototype.x++ === 9) {
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

let foo = new Foo();
for (let i = 0; i < 11; i++)
    foo.y = 42;
