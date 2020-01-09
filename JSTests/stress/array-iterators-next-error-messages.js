function assert(a, b) {
    if (a != b)
        throw new Error("assertion failed");
}

next = [].values().next;

try {
    next.call(null);
} catch(e) {
    assert(e, "TypeError: %ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance");
}

try {
    next.call(undefined);
} catch(e) {
    assert(e, "TypeError: %ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance");
}
