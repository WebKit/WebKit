function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const alien_global_object = createGlobalObject();

const a = {};
const b = alien_global_object.Object();

a.__proto__ = b;

function cons() {

}

cons.prototype = a;

// Cache
Reflect.construct(Array, [1.1, 2.2, 3.3], cons);

// Clear rareData to avoid the check in ObjectsWithBrokenIndexingFinder<mode>::visit(JSObject* object).
cons.prototype = null;
cons.prototype = a;

// Have a bad time.
b.__proto__ = new Proxy({}, {});

// This will create a double array having a Proxy object in its prototype chain.
shouldBe(!!describe(Reflect.construct(Array, [1.1, 2.2, 3.3], cons)).match(/ArrayWithSlowPutArrayStorage/), true);
