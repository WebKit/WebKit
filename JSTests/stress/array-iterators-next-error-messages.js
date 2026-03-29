function assert(kind, a, b) {
    if (a != b)
        throw new Error(kind + ": assertion failed");
}

function test(kind, iterator) {
    let next = iterator.next;
    try {
        next.call(null);
    } catch(e) {
        assert(kind, e, "TypeError: %ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance");
    }

    try {
        next.call(undefined);
    } catch(e) {
        assert(kind, e, "TypeError: %ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance");
    }
}

test("keys", [].keys());
test("values", [].values());
test("entries", [].entries());