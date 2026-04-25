function assert(b) {
    if (!b)
        throw new Error("Bad!")
}

let set = new Set;
for (let i = 0; i < 50000; i++) {
    assert(set.size === i);
    set.add(i);
    assert(set.has(i));
}

for (let i = 0; i < 50000; i++) {
    assert(set.size === 50000 - i);
    set.delete(i);
    assert(!set.has(i));
}

assert(!set.size);
