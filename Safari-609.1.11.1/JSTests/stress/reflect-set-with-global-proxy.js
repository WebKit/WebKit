function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
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

// The global object is wrapped with the JSProxy.
var global = this;

receiverTest(global, {});
receiverTestIndexed(global, {});
Reflect.defineProperty(global, 'OK1', {
    set: function (value) {
        shouldBe(this, global);
    }
});
function test1()
{
    global.OK1 = 'Hello';
}
noInline(test1);
for (var i = 0; i < 1e4; ++i)
    test1();

Reflect.defineProperty(global, 'OK2', {
    set: function (value) {
        'use strict';
        shouldBe(this, global);
    }
});
function test2()
{
    global.OK2 = 'Hello';
}
noInline(test2);
for (var i = 0; i < 1e4; ++i)
    test2();

var receiver = {};
Reflect.defineProperty(global, 'OK3', {
    set: function (value) {
        shouldBe(this, receiver);
    }
});
function test3()
{
    shouldBe(Reflect.set(global, 'OK3', 'value', receiver), true);
}
noInline(test3);
for (var i = 0; i < 1e4; ++i)
    test3();
