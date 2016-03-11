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
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

shouldBe(Reflect.set.length, 3);

shouldThrow(() => {
    Reflect.set("hello", "hello", 42);
}, `TypeError: Reflect.set requires the first argument be an object`);

var symbol = Symbol();

(function receiverCase() {
    // FIXME: Currently, receiver is not supported.
    var receiver = {};
    var object = {
    };

    shouldBe(Reflect.set(object, 'Cocoa', 42, receiver), true);
    // shouldBe(Reflect.get(object, 'Cocoa'), undefined);
    // shouldBe(Reflect.get(receiver, 'Cocoa'), 42);

    var object2 = {
        set Cocoa(value) {
            print(value);
            shouldBe(this, receiver);
            this.value = value;
        }
    };
    shouldBe(Reflect.set(object, 'Cocoa', 42, receiver), true);
    // shouldBe(Reflect.get(object, 'Cocoa'), undefined);
    // shouldBe(Reflect.get(receiver, 'Cocoa'), 42);
}());

(function proxyCase() {
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
    object[1000000] = "Hello";
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
    object[1000000] = "Hello";
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
    object[1000000] = "Hello";
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
}());

(function stringObjectCase() {
    'use strict';
    var object = new String("Cocoa");
    shouldBe(Reflect.get(object, 'hello'), undefined);
    shouldBe(Reflect.set(object, 'hello', 42), true);
    shouldBe(Reflect.get(object, 'hello'), 42);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.set(object, 0, 42), false);
    shouldBe(Reflect.get(object, 0), 'C');
    shouldBe(Reflect.get(object, symbol), undefined);
    shouldBe(Reflect.set(object, symbol, 42), true);
    shouldBe(Reflect.get(object, symbol), 42);
    object[1000000] = "Cocoa";
    shouldBe(Reflect.set(object, 0, 50), false);
    shouldBe(Reflect.get(object, 0), 'C');

    var object = new String("Cocoa");
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
    var object = new String("Cocoa");
    shouldBe(Reflect.get(object, 'length'), 5);
    shouldBe(Reflect.set(object, 'length', 'Cappuccino'), false);
    shouldBe(Reflect.get(object, 'length'), 5);

    var object = new String("Cocoa");
    shouldBe(Reflect.get(object, 'length'), 5);
    shouldBe(Reflect.set(object, 'length', 20), false);
    shouldBe(Reflect.get(object, 'length'), 5);

    var object = new String("Cocoa");
    Object.freeze(object);
    shouldBe(Reflect.get(object, 'length'), 5);
    shouldBe(Reflect.set(object, 'length', 20), false);
    shouldBe(Reflect.get(object, 'length'), 5);

    var object = new String("Cocoa");
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

    var object = new String("Cocoa");
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
}());

(function customSetter() {
    // In this case, RegExp.multiline behaves like a setter because it coerce boolean type.
    // Anyway, it's OK, because RegExp.multiline is not specified in the spec.

    function test1() {
        shouldBe(Reflect.set(RegExp, 'multiline', 'Cappuccino'), true);
        shouldBe(Reflect.get(RegExp, 'multiline'), true);
        shouldBe(Reflect.set(RegExp, 'multiline', 0), true);
        shouldBe(Reflect.get(RegExp, 'multiline'), false);
    }

    function test2() {
        'use strict';
        shouldBe(Reflect.set(RegExp, 'multiline', 'Cappuccino'), true);
        shouldBe(Reflect.get(RegExp, 'multiline'), true);
        shouldBe(Reflect.set(RegExp, 'multiline', 0), true);
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

    shouldBe(Reflect.defineProperty(regexp, 'lastIndex', {
        value: 42,
        writable: false
    }), true);
    shouldBe(Reflect.get(regexp, 'lastIndex'), 42);
    shouldBe(Reflect.set(regexp, 'lastIndex', 'Hello'), false);
    shouldBe(Reflect.get(regexp, 'lastIndex'), 42);

    shouldThrow(function () {
        'use strict';
        regexp.lastIndex = "NG";
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
}());
