function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('not thrown.');
    if (String(error) !== message)
        throw new Error('bad error: ' + String(error));
}

function unreachable()
{
    throw new Error('unreachable');
}

function receiverTest(object, receiver)
{
    shouldBe(Reflect.set(object, 'Cocoa', 42, receiver), true);
    shouldBe(Reflect.get(receiver, 'Cocoa'), 42);
    shouldBe(Reflect.get(object, 'Cocoa'), undefined);

    // Existing.
    shouldBe(Reflect.set(object, 'Matcha', 40), true);
    shouldBe(Reflect.get(object, 'Matcha'), 40);
    shouldBe(Reflect.set(object, 'Matcha', 42, receiver), true);
    shouldBe(Reflect.get(receiver, 'Matcha'), 42);
    shouldBe(Reflect.get(object, 'Matcha'), 40);

    // Existing non writable own descriptor.
    Reflect.defineProperty(object, 'Cappuccino', {
        value: 'nice',
        writable: false
    });
    shouldBe(Reflect.set(object, 'Cappuccino', 42, receiver), false);
    shouldBe(Reflect.get(receiver, 'Cappuccino'), undefined);
    shouldBe(receiver.hasOwnProperty('Cappuccino'), false);
    shouldBe(Reflect.get(object, 'Cappuccino'), 'nice');

    // Existing non writable receiver descriptor.
    Reflect.defineProperty(receiver, 'Kilimanjaro', {
        value: 'good',
        writable: false
    });
    shouldBe(Reflect.set(object, 'Kilimanjaro', 42, receiver), false);
    shouldBe(Reflect.get(receiver, 'Kilimanjaro'), 'good');
    shouldBe(receiver.hasOwnProperty('Kilimanjaro'), true);
    shouldBe(Reflect.get(object, 'Kilimanjaro'), undefined);
    shouldBe(object.hasOwnProperty('Kilimanjaro'), false);

    shouldBe(Reflect.set(object, 'Kilimanjaro', 42, 'receiver'), false);

    // Receiver accessors.
    shouldBe(Reflect.defineProperty(receiver, 'Mocha', {
        get()
        {
            return 42;
        },
        configurable: true
    }), true);
    shouldBe(Reflect.set(object, 'Mocha', 42, receiver), false);
    shouldBe(Reflect.defineProperty(receiver, 'Mocha', {
        set(value)
        {
            unreachable();
        },
        configurable: true
    }), true);
    shouldBe(Reflect.set(object, 'Mocha', 42, receiver), false);
    shouldBe(receiver.value, undefined);
    shouldBe(Reflect.defineProperty(object, 'Mocha', {
        get(value)
        {
        },
        configurable: true
    }), true);
    shouldBe(Reflect.set(object, 'Mocha', 42, receiver), false);
    shouldBe(receiver.value, undefined);
    shouldBe(Reflect.defineProperty(object, 'Mocha', {
        set(value)
        {
            shouldBe(this, receiver);
            this.value = value;
        },
        configurable: true
    }), true);
    shouldBe(Reflect.set(object, 'Mocha', 42, receiver), true);
    shouldBe(receiver.value, 42);
    shouldBe(object.value, undefined);
}

function receiverTestIndexed(object, receiver)
{
    shouldBe(Reflect.set(object, 11, 42, receiver), true);
    shouldBe(Reflect.get(receiver, 11), 42);
    shouldBe(Reflect.get(object, 11), undefined);

    // Existing.
    shouldBe(Reflect.set(object, 12, 40), true);
    shouldBe(Reflect.get(object, 12), 40);
    shouldBe(Reflect.set(object, 12, 42, receiver), true);
    shouldBe(Reflect.get(receiver, 12), 42);
    shouldBe(Reflect.get(object, 12), 40);

    // Existing non writable own descriptor.
    Reflect.defineProperty(object, 13, {
        value: 'nice',
        writable: false
    });
    shouldBe(Reflect.set(object, 13, 42, receiver), false);
    shouldBe(Reflect.get(receiver, 13), undefined);
    shouldBe(receiver.hasOwnProperty(13), false);
    shouldBe(Reflect.get(object, 13), 'nice');

    // Existing non writable receiver descriptor.
    Reflect.defineProperty(receiver, 14, {
        value: 'good',
        writable: false
    });
    shouldBe(Reflect.set(object, 14, 42, receiver), false);
    shouldBe(Reflect.get(receiver, 14), 'good');
    shouldBe(receiver.hasOwnProperty(14), true);
    shouldBe(Reflect.get(object, 14), undefined);
    shouldBe(object.hasOwnProperty(14), false);

    // Receiver is a primitive value.
    shouldBe(Reflect.set(object, 14, 42, 'receiver'), false);

    // Receiver accessors.
    shouldBe(Reflect.defineProperty(receiver, 15, {
        get()
        {
            return 42;
        },
        configurable: true
    }), true);
    shouldBe(Reflect.set(object, 15, 42, receiver), false);
    shouldBe(Reflect.defineProperty(receiver, 15, {
        set(value)
        {
            unreachable();
        },
        configurable: true
    }), true);
    shouldBe(Reflect.set(object, 15, 42, receiver), false);
    shouldBe(receiver.value, undefined);
    shouldBe(Reflect.defineProperty(object, 15, {
        get(value)
        {
        },
        configurable: true
    }), true);
    shouldBe(Reflect.set(object, 15, 42, receiver), false);
    shouldBe(receiver.value, undefined);
    shouldBe(Reflect.defineProperty(object, 15, {
        set(value)
        {
            shouldBe(this, receiver);
            this.value = value;
        },
        configurable: true
    }), true);
    shouldBe(Reflect.set(object, 15, 42, receiver), true);
    shouldBe(receiver.value, 42);
    shouldBe(object.value, undefined);
}

shouldBe(Reflect.set.length, 3);

shouldThrow(() => {
    Reflect.set('hello', 'hello', 42);
}, `TypeError: Reflect.set requires the first argument be an object`);

var symbol = Symbol();

(function simpleReceiverCase() {
    var receiver = {};
    var object = {
    };

    shouldBe(Reflect.set(object, 'Cocoa', 42, receiver), true);
    shouldBe(Reflect.get(object, 'Cocoa'), undefined);
    shouldBe(Reflect.get(receiver, 'Cocoa'), 42);

    var object2 = {
        set Cocoa(value) {
            print(value);
            shouldBe(this, receiver);
            this.value = value;
        }
    };
    shouldBe(Reflect.set(object, 'Cocoa', 42, receiver), true);
    shouldBe(Reflect.get(object, 'Cocoa'), undefined);
    shouldBe(Reflect.get(receiver, 'Cocoa'), 42);
}());

(function objectCase() {
    'use strict';
    var object = {};
    shouldBe(Reflect.get(object, 'hello'), undefined);
    shouldBe(Reflect.set(object, 'hello', 42), true);
    shouldBe(Reflect.get(object, 'hello'), 42);
    shouldBe(Reflect.get(object, 0), undefined);
    shouldBe(Reflect.set(object, 0, 42), true);
    shouldBe(Reflect.get(object, 0), 42);
    shouldBe(Reflect.get(object, symbol), undefined);
    shouldBe(Reflect.set(object, symbol, 42), true);
    shouldBe(Reflect.get(object, symbol), 42);

    var object = {};
    shouldBe(Reflect.defineProperty(object, 'hello', {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, symbol, {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    var object = {};
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    var object = {};
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    receiverTest({}, {});
    receiverTestIndexed({}, {});
}());

(function arrayCase() {
    'use strict';
    var object = [];
    shouldBe(Reflect.get(object, 'hello'), undefined);
    shouldBe(Reflect.set(object, 'hello', 42), true);
    shouldBe(Reflect.get(object, 'hello'), 42);
    shouldBe(Reflect.get(object, 0), undefined);
    shouldBe(Reflect.set(object, 0, 42), true);
    shouldBe(Reflect.get(object, 0), 42);
    shouldBe(Reflect.get(object, symbol), undefined);
    shouldBe(Reflect.set(object, symbol, 42), true);
    shouldBe(Reflect.get(object, symbol), 42);
    object[1000000] = 'Hello';
    shouldBe(Reflect.set(object, 0, 50), true);
    shouldBe(Reflect.get(object, 0), 50);

    var object = [];
    shouldBe(Reflect.defineProperty(object, 'hello', {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(object.length, 1);
    shouldBe(Reflect.defineProperty(object, symbol, {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    // Length.
    var object = [];
    shouldBe(Reflect.get(object, 'length'), 0);
    shouldThrow(() => {
        Reflect.set(object, 'length', 'Cappuccino');
    }, `RangeError: Invalid array length`);
    shouldBe(Reflect.get(object, 'length'), 0);

    var object = [];
    shouldBe(Reflect.get(object, 'length'), 0);
    shouldBe(Reflect.set(object, 'length', 20), true);
    shouldBe(Reflect.get(object, 'length'), 20);

    var object = [];
    Object.freeze(object);
    shouldBe(Reflect.get(object, 'length'), 0);
    shouldBe(Reflect.set(object, 'length', 20), false);
    shouldBe(Reflect.get(object, 'length'), 0);

    var object = [];
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    var object = [];
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    receiverTest([], []);
    receiverTest({}, []);
    receiverTest([], {});
    receiverTestIndexed([], []);
    receiverTestIndexed({}, []);
    receiverTestIndexed([], {});

    var array = [0, 1, 2, 3];
    var receiver = {};
    shouldBe(Reflect.set(array, 'length', 'V', receiver), true);
    shouldBe(Reflect.get(receiver, 'length'), 'V');
    shouldBe(receiver.hasOwnProperty('length'), true);
    shouldBe(Reflect.get(array, 'length'), 4);
    Object.freeze(array);
    var receiver = {};
    shouldBe(Reflect.set(array, 'length', 'V', receiver), false);
    shouldBe(Reflect.get(receiver, 'length'), undefined);
    shouldBe(receiver.hasOwnProperty('length'), false);
    shouldBe(Reflect.get(array, 'length'), 4);
}());

(function arrayBufferCase() {
    'use strict';
    var object = new ArrayBuffer(64);
    shouldBe(Reflect.get(object, 'hello'), undefined);
    shouldBe(Reflect.set(object, 'hello', 42), true);
    shouldBe(Reflect.get(object, 'hello'), 42);
    shouldBe(Reflect.get(object, 0), undefined);
    shouldBe(Reflect.set(object, 0, 42), true);
    shouldBe(Reflect.get(object, 0), 42);
    shouldBe(Reflect.get(object, symbol), undefined);
    shouldBe(Reflect.set(object, symbol, 42), true);
    shouldBe(Reflect.get(object, symbol), 42);
    object[1000000] = 'Hello';
    shouldBe(Reflect.set(object, 0, 50), true);
    shouldBe(Reflect.get(object, 0), 50);

    var object = new ArrayBuffer(64);
    shouldBe(Reflect.defineProperty(object, 'hello', {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(object.length, undefined);
    shouldBe(Reflect.defineProperty(object, symbol, {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    var object = new ArrayBuffer(64);
    shouldBe(Reflect.get(object, 'byteLength'), 64);
    shouldBe(Reflect.set(object, 'byteLength', 'Cappuccino'), false);
    shouldBe(Reflect.get(object, 'byteLength'), 64);

    var object = new ArrayBuffer(64);
    shouldBe(Reflect.get(object, 'byteLength'), 64);
    shouldBe(Reflect.set(object, 'byteLength', 2000), false);
    shouldBe(Reflect.get(object, 'byteLength'), 64);

    var object = new ArrayBuffer(64);
    shouldBe(Reflect.defineProperty(object, 'byteLength', {
        writable: false
    }), false);
    shouldBe(Reflect.get(object, 'byteLength'), 64);
    shouldBe(Reflect.set(object, 'byteLength', 20), false);
    shouldBe(Reflect.get(object, 'byteLength'), 64);

    var object = new ArrayBuffer(64);
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    var object = new ArrayBuffer(64);
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    receiverTest(new ArrayBuffer(64), new ArrayBuffer(64));
    receiverTest({}, new ArrayBuffer(64));
    receiverTest(new ArrayBuffer(64), {});
    receiverTestIndexed(new ArrayBuffer(64), new ArrayBuffer(64));
    receiverTestIndexed({}, new ArrayBuffer(64));
    receiverTestIndexed(new ArrayBuffer(64), {});
}());

[
    [ Uint8Array, 1 ],
    [ Uint8ClampedArray, 1],
    [ Uint16Array, 2 ],
    [ Uint32Array, 4 ],
    [ Int8Array, 1 ],
    [ Int16Array, 2 ],
    [ Int32Array, 4 ],
    [ Float32Array, 4 ],
    [ Float64Array, 8 ],
].forEach(function typedArrayCase([ TypedArray, bytesPerElement ]) {
    'use strict';
    var object = new TypedArray(64);
    shouldBe(Reflect.get(object, 'hello'), undefined);
    shouldBe(Reflect.set(object, 'hello', 42), true);
    shouldBe(Reflect.get(object, 'hello'), 42);
    shouldBe(Reflect.get(object, 0), 0);
    shouldBe(Reflect.set(object, 0, 42), true);
    shouldBe(Reflect.get(object, 0), 42);
    shouldBe(Reflect.get(object, symbol), undefined);
    shouldBe(Reflect.set(object, symbol, 42), true);
    shouldBe(Reflect.get(object, symbol), 42);
    object[1000000] = 'Hello';
    shouldBe(Reflect.set(object, 0, 50), true);
    shouldBe(Reflect.get(object, 0), 50);

    var object = new TypedArray(64);
    shouldBe(Reflect.defineProperty(object, 'hello', {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        value: 'Cocoa',
        writable: false
    }), false);
    shouldBe(Reflect.get(object, 0), 0);
    shouldBe(Reflect.set(object, 0, 42), true);
    shouldBe(Reflect.get(object, 0), 42);
    shouldBe(object.length, 64);
    shouldBe(Reflect.defineProperty(object, symbol, {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    var object = new TypedArray(64);
    shouldBe(Reflect.get(object, 'byteLength'), bytesPerElement * 64);
    shouldBe(Reflect.set(object, 'byteLength', 'Cappuccino'), false);
    shouldBe(Reflect.get(object, 'byteLength'), bytesPerElement * 64);

    var object = new TypedArray(64);
    shouldBe(Reflect.get(object, 'byteLength'), bytesPerElement * 64);
    shouldBe(Reflect.set(object, 'byteLength', 2000), false);
    shouldBe(Reflect.get(object, 'byteLength'), bytesPerElement * 64);

    var object = new TypedArray(64);
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), false);
    shouldBe(Reflect.get(object, 0), 0);
    shouldBe(Reflect.set(object, 0, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 0), 42);
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    var object = new TypedArray(64);
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        }
    }), false);
    shouldBe(Reflect.get(object, 0), 0);
    shouldBe(Reflect.set(object, 0, 42), true);
    shouldBe(Reflect.get(object, 0), 42);
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    receiverTest(new TypedArray(64), new TypedArray(64));
    receiverTest(new TypedArray(64), {});
    receiverTest({}, new TypedArray(64));

    var object = new TypedArray(64);
    var receiver = {};
    // The receiver is ignored when the property name is an indexed one.
    // shouldBe(Reflect.set(object, 0, 42, receiver), true);
    shouldBe(Reflect.set(object, 0, 42, receiver), true);
    shouldBe(Reflect.get(object, 0), 42);
    shouldBe(receiver.hasOwnProperty(0), false);
});


(function argumentCase() {
    function test1() {
        var object = arguments;
        shouldBe(Reflect.get(object, 'hello'), undefined);
        shouldBe(Reflect.set(object, 'hello', 42), true);
        shouldBe(Reflect.get(object, 'hello'), 42);
        shouldBe(Reflect.get(object, 0), undefined);
        shouldBe(Reflect.set(object, 0, 42), true);
        shouldBe(Reflect.get(object, 0), 42);
        shouldBe(Reflect.get(object, symbol), undefined);
        shouldBe(Reflect.set(object, symbol, 42), true);
        shouldBe(Reflect.get(object, symbol), 42);
    }

    function test2() {
        var object = arguments;
        shouldBe(Reflect.defineProperty(object, 'hello', {
            value: 'Cocoa',
            writable: false
        }), true);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.set(object, 'hello', 42), false);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, 0, {
            value: 'Cocoa',
            writable: false
        }), true);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.set(object, 0, 42), false);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(object.length, 0);
        shouldBe(Reflect.defineProperty(object, symbol, {
            value: 'Cocoa',
            writable: false
        }), true);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
        shouldBe(Reflect.set(object, symbol, 42), false);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
    }

    function test3() {
        var object = arguments;
        shouldBe(Reflect.get(object, 'length'), 0);
        Reflect.set(object, 'length', 'Cappuccino');
        shouldBe(Reflect.get(object, 'length'), 'Cappuccino');

        Reflect.set(object, 'callee', 'Cappuccino');
        shouldBe(Reflect.get(object, 'callee'), 'Cappuccino');
    }

    function test4() {
        var object = arguments;
        shouldBe(Reflect.defineProperty(object, 'hello', {
            get() {
                return 'Cocoa';
            },
            set() {
            }
        }), true);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.set(object, 'hello', 42), true);  // Return true since the setter exists.
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, 0, {
            get() {
                return 'Cocoa';
            },
            set() {
            }
        }), true);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.set(object, 0, 42), true);  // Return true since the setter exists.
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, symbol, {
            get() {
                return 'Cocoa';
            },
            set() {
            }
        }), true);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
        shouldBe(Reflect.set(object, symbol, 42), true);  // Return true since the setter exists.
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
    }

    function test5() {
        var object = arguments;
        shouldBe(Reflect.defineProperty(object, 'hello', {
            get() {
                return 'Cocoa';
            }
        }), true);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.set(object, 'hello', 42), false);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, 0, {
            get() {
                return 'Cocoa';
            }
        }), true);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.set(object, 0, 42), false);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, symbol, {
            get() {
                return 'Cocoa';
            }
        }), true);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
        shouldBe(Reflect.set(object, symbol, 42), false);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
    }

    test1();
    test2();
    test3();
    test4();
    test5();

    function getArguments() { return arguments; }

    receiverTest(getArguments(), getArguments());
    receiverTest({}, getArguments());
    receiverTest(getArguments(), {});
    receiverTestIndexed(getArguments(), getArguments());
    receiverTestIndexed({}, getArguments());
    receiverTestIndexed(getArguments(), {});

    var args = getArguments(0, 1, 2);
    var receiver = {};
    shouldBe(Reflect.set(args, 0, 'V', receiver), true);
    shouldBe(Reflect.get(receiver, 0), 'V');
    shouldBe(Reflect.set(args, 'length', 'V', receiver), true);
    shouldBe(Reflect.get(receiver, 'length'), 'V');
}());

(function argumentStrictCase() {
    'use strict';
    function test1() {
        var object = arguments;
        shouldBe(Reflect.get(object, 'hello'), undefined);
        shouldBe(Reflect.set(object, 'hello', 42), true);
        shouldBe(Reflect.get(object, 'hello'), 42);
        shouldBe(Reflect.get(object, 0), undefined);
        shouldBe(Reflect.set(object, 0, 42), true);
        shouldBe(Reflect.get(object, 0), 42);
        shouldBe(Reflect.get(object, symbol), undefined);
        shouldBe(Reflect.set(object, symbol, 42), true);
        shouldBe(Reflect.get(object, symbol), 42);
    }

    function test2() {
        var object = arguments;
        shouldBe(Reflect.defineProperty(object, 'hello', {
            value: 'Cocoa',
            writable: false
        }), true);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.set(object, 'hello', 42), false);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, 0, {
            value: 'Cocoa',
            writable: false
        }), true);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.set(object, 0, 42), false);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(object.length, 0);
        shouldBe(Reflect.defineProperty(object, symbol, {
            value: 'Cocoa',
            writable: false
        }), true);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
        shouldBe(Reflect.set(object, symbol, 42), false);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
    }

    function test3() {
        var object = arguments;
        shouldBe(Reflect.get(object, 'length'), 0);
        Reflect.set(object, 'length', 'Cappuccino');
        shouldBe(Reflect.get(object, 'length'), 'Cappuccino');

        shouldThrow(() => {
            Reflect.set(object, 'callee', 'Cappuccino');
        }, `TypeError: Type error`);
        shouldThrow(() => {
            Reflect.get(object, 'callee');
        }, `TypeError: Type error`);
    }

    function test4() {
        var object = arguments;
        shouldBe(Reflect.defineProperty(object, 'hello', {
            get() {
                return 'Cocoa';
            },
            set() {
            }
        }), true);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.set(object, 'hello', 42), true);  // Return true since the setter exists.
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, 0, {
            get() {
                return 'Cocoa';
            },
            set() {
            }
        }), true);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.set(object, 0, 42), true);  // Return true since the setter exists.
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, symbol, {
            get() {
                return 'Cocoa';
            },
            set() {
            }
        }), true);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
        shouldBe(Reflect.set(object, symbol, 42), true);  // Return true since the setter exists.
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
    }

    function test5() {
        var object = arguments;
        shouldBe(Reflect.defineProperty(object, 'hello', {
            get() {
                return 'Cocoa';
            }
        }), true);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.set(object, 'hello', 42), false);
        shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, 0, {
            get() {
                return 'Cocoa';
            }
        }), true);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.set(object, 0, 42), false);
        shouldBe(Reflect.get(object, 0), 'Cocoa');
        shouldBe(Reflect.defineProperty(object, symbol, {
            get() {
                return 'Cocoa';
            }
        }), true);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
        shouldBe(Reflect.set(object, symbol, 42), false);
        shouldBe(Reflect.get(object, symbol), 'Cocoa');
    }

    test1();
    test2();
    test3();
    test4();
    test5();

    function getArguments() { return arguments; }

    receiverTest(getArguments(), getArguments());
    receiverTest({}, getArguments());
    receiverTest(getArguments(), {});
    receiverTestIndexed(getArguments(), getArguments());
    receiverTestIndexed({}, getArguments());
    receiverTestIndexed(getArguments(), {});

    var args = getArguments(0, 1, 2);
    var receiver = {};
    shouldBe(Reflect.set(args, 0, 'V', receiver), true);
    shouldBe(Reflect.get(receiver, 0), 'V');
    shouldBe(Reflect.set(args, 'length', 'V', receiver), true);
    shouldBe(Reflect.get(receiver, 'length'), 'V');
}());

(function stringObjectCase() {
    'use strict';
    var object = new String('Cocoa');
    shouldBe(Reflect.get(object, 'hello'), undefined);
    shouldBe(Reflect.set(object, 'hello', 42), true);
    shouldBe(Reflect.get(object, 'hello'), 42);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.get(object, symbol), undefined);
    shouldBe(Reflect.set(object, symbol, 42), true);
    shouldBe(Reflect.get(object, symbol), 42);
    object[1000000] = 'Cocoa';
    shouldBe(Reflect.set(object, 0, 50), false);
    shouldBe(Reflect.get(object, 0), 'C');

    var object = new String('Cocoa');
    shouldBe(Reflect.defineProperty(object, 'hello', {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        value: 'Cocoa',
        writable: false
    }), false);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(object.length, 5);
    shouldBe(Reflect.defineProperty(object, symbol, {
        value: 'Cocoa',
        writable: false
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    // Length.
    var object = new String('Cocoa');
    shouldBe(Reflect.get(object, 'length'), 5);
    shouldBe(Reflect.set(object, 'length', 'Cappuccino'), false);
    shouldBe(Reflect.get(object, 'length'), 5);

    var object = new String('Cocoa');
    shouldBe(Reflect.get(object, 'length'), 5);
    shouldBe(Reflect.set(object, 'length', 20), false);
    shouldBe(Reflect.get(object, 'length'), 5);

    var object = new String('Cocoa');
    Object.freeze(object);
    shouldBe(Reflect.get(object, 'length'), 5);
    shouldBe(Reflect.set(object, 'length', 20), false);
    shouldBe(Reflect.get(object, 'length'), 5);

    var object = new String('Cocoa');
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), false);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.set(object, 0, 42), false);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        },
        set() {
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), true);  // Return true since the setter exists.
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    var object = new String('Cocoa');
    shouldBe(Reflect.defineProperty(object, 'hello', {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.set(object, 'hello', 42), false);
    shouldBe(Reflect.get(object, 'hello'), 'Cocoa');
    shouldBe(Reflect.defineProperty(object, 0, {
        get() {
            return 'Cocoa';
        }
    }), false);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.defineProperty(object, symbol, {
        get() {
            return 'Cocoa';
        }
    }), true);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');
    shouldBe(Reflect.set(object, symbol, 42), false);
    shouldBe(Reflect.get(object, symbol), 'Cocoa');

    receiverTest(new String('Hello'), new String('World'));
    receiverTest({}, new String('World'));
    receiverTest(new String('Hello'), {});
    // Tested indice should be out of range of the string object.
    receiverTestIndexed(new String('Hello'), new String('World'));
    receiverTestIndexed({}, new String('World'));
    receiverTestIndexed(new String('Hello'), {});

    var string = new String('Hello');
    var receiver = {};
    shouldBe(Reflect.set(string, 0, 'V', receiver), false);
    shouldBe(Reflect.get(receiver, 0), undefined);
    shouldBe(receiver.hasOwnProperty(0), false);
    shouldBe(Reflect.set(string, 'length', 'V', receiver), false);
    shouldBe(Reflect.get(receiver, 'length'), undefined);
    shouldBe(receiver.hasOwnProperty('length'), false);
}());

(function regExpCase() {
    receiverTest(/hello/, /world/);
    receiverTest({}, /world/);
    receiverTest(/hello/, {});
    receiverTestIndexed(/hello/, /world/);
    receiverTestIndexed({}, /world/);
    receiverTestIndexed(/hello/, {});
}());

(function customValue() {
    // In this case, RegExp.multiline behaves like a setter because it coerce boolean type.
    // Anyway, it's OK, because RegExp.multiline is not specified in the spec.

    function test1() {
        shouldBe(Reflect.set(RegExp, 'multiline', 'Cappuccino'), true);
        shouldBe(Reflect.get(RegExp, 'multiline'), true);
        shouldBe(Reflect.set(RegExp, 'multiline', 0), true);
        shouldBe(Reflect.get(RegExp, 'multiline'), false);

        var receiver = {};
        shouldBe(Reflect.set(RegExp, 'multiline', 'Cappuccino', receiver), true);
        shouldBe(Reflect.get(receiver, 'multiline'), 'Cappuccino');
        shouldBe(Reflect.get(RegExp, 'multiline'), false);
    }

    function test2() {
        'use strict';
        shouldBe(Reflect.set(RegExp, 'multiline', 'Cappuccino'), true);
        shouldBe(Reflect.get(RegExp, 'multiline'), true);
        shouldBe(Reflect.set(RegExp, 'multiline', 0), true);
        shouldBe(Reflect.get(RegExp, 'multiline'), false);

        var receiver = {};
        shouldBe(Reflect.set(RegExp, 'multiline', 'Cappuccino', receiver), true);
        shouldBe(Reflect.get(receiver, 'multiline'), 'Cappuccino');
        shouldBe(Reflect.get(RegExp, 'multiline'), false);
    }

    function test3() {
        'use strict';
        shouldBe(Reflect.defineProperty(RegExp, 'multiline', {
            writable: false
        }), true);
        shouldBe(Reflect.get(RegExp, 'multiline'), false);
        shouldBe(Reflect.set(RegExp, 'multiline', 'Cappuccino'), false);
        shouldBe(Reflect.get(RegExp, 'multiline'), false);
        shouldBe(Reflect.set(RegExp, 'multiline', 0), false);
        shouldBe(Reflect.get(RegExp, 'multiline'), false);

        var receiver = {};
        shouldBe(Reflect.set(RegExp, 'multiline', 'Cappuccino', receiver), false);
        shouldBe(Reflect.get(receiver, 'multiline'), undefined);
    }

    test1();
    test2();
    test3();
}());

(function regExpLastIndex() {
    var regexp = new RegExp('Cocoa');
    regexp.lastIndex = 'Hello';
    shouldBe(Reflect.get(regexp, 'lastIndex'), 'Hello');
    regexp.lastIndex = 42;
    shouldBe(Reflect.get(regexp, 'lastIndex'), 42);

    shouldBe(Reflect.set(regexp, 'lastIndex', 'Hello'), true);
    shouldBe(Reflect.get(regexp, 'lastIndex'), 'Hello');

    var receiver = {};
    shouldBe(Reflect.set(regexp, 'lastIndex', 'Cocoa', receiver), true);
    shouldBe(Reflect.get(receiver, 'lastIndex'), 'Cocoa');
    shouldBe(Reflect.get(regexp, 'lastIndex'), 'Hello');

    shouldBe(Reflect.defineProperty(regexp, 'lastIndex', {
        value: 42,
        writable: false
    }), true);
    shouldBe(Reflect.get(regexp, 'lastIndex'), 42);
    shouldBe(Reflect.set(regexp, 'lastIndex', 'Hello'), false);
    shouldBe(Reflect.get(regexp, 'lastIndex'), 42);

    var receiver = {};
    shouldBe(Reflect.set(regexp, 'lastIndex', 'Cocoa', receiver), false);
    shouldBe(Reflect.get(receiver, 'lastIndex'), undefined);
    shouldBe(Reflect.get(regexp, 'lastIndex'), 42);

    shouldThrow(function () {
        'use strict';
        regexp.lastIndex = 'NG';
    }, `TypeError: Attempted to assign to readonly property.`);
}());

(function functionCase() {
    var func = function () { };
    shouldBe(Reflect.get(func, 'arguments'), null);
    shouldBe(Reflect.set(func, 'arguments', 42), false);
    shouldBe(Reflect.get(func, 'arguments'), null);

    shouldBe(Reflect.get(func, 'caller'), null);
    shouldBe(Reflect.set(func, 'caller', 42), false);
    shouldBe(Reflect.get(func, 'caller'), null);

    receiverTest(function () {}, function () {});
    receiverTest({}, function () {});
    receiverTest(function () {}, {});
    receiverTestIndexed(function () {}, function () {});
    receiverTestIndexed({}, function () {});
    receiverTestIndexed(function () {}, {});

    var receiver = {};
    shouldBe(Reflect.set(func, 'arguments', 'V', receiver), false);
    shouldBe(Reflect.get(receiver, 'arguments'), undefined);
    shouldBe(receiver.hasOwnProperty('arguments'), false);
    shouldBe(Reflect.get(func, 'arguments'), null);
    shouldBe(Reflect.set(func, 'caller', 'V', receiver), false);
    shouldBe(Reflect.get(receiver, 'caller'), undefined);
    shouldBe(receiver.hasOwnProperty('caller'), false);
    shouldBe(Reflect.get(func, 'caller'), null);
}());
