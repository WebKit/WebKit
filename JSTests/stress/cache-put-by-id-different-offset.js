//@ runNoLLInt

function Foo() { }

Foo.prototype.x = 0;

Object.defineProperty(Foo.prototype, 'y', {
    set(x) {
        if (Foo.prototype.x++ === 1) {
            delete Foo.prototype.x;
            if (typeof $vm !== 'undefined')
                $vm.flattenDictionaryObject(Foo.prototype);
        }
    }
});

let foo = new Foo();
while (typeof foo.x === 'number')
    foo.y = 42;
