function assert(a) {
    if (!a)
        throw Error("Bad assertion!");
}

function tryChangeNonConfigurableDescriptor(x) {
    Object.defineProperty(arguments, 0, {configurable: false});
    try {
        Object.defineProperty(arguments, 0, x);
        assert(false);
    } catch(e) {
        assert(e instanceof TypeError);
    }
}

tryChangeNonConfigurableDescriptor({get: () => {return 50;} });
tryChangeNonConfigurableDescriptor({set: (x) => {}});
tryChangeNonConfigurableDescriptor({writable: true, enumerable: false});

function tryChangeWritableOfNonConfigurableDescriptor(x) {
    Object.defineProperty(arguments, 0, {configurable: false});
    Object.defineProperty(arguments, 0, {writable: true});
    assert(Object.defineProperty(arguments, 0, {writable: false}));
}

tryChangeWritableOfNonConfigurableDescriptor("foo");

