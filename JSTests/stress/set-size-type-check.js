function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        if (String(e) !== errorMessage)
            throw new Error('bad error: ' + String(e));
    }
    if (!errorThrown)
        throw new Error('not thrown');
}

var sizeGetter = Object.getOwnPropertyDescriptor(Set.prototype, 'size').get;

function getSize(obj) {
    return sizeGetter.call(obj);
}
noInline(getSize);

// Compile with Set objects
for (var i = 0; i < testLoopCount; ++i) {
    var s = new Set([1, 2, 3]);
    if (getSize(s) !== 3)
        throw new Error('bad size');
}

// Type check failure (OSR exit)
shouldThrow(() => { getSize(new Map()); }, "TypeError: Set operation called on non-Set object");
shouldThrow(() => { getSize({}); }, "TypeError: Set operation called on non-Set object");
