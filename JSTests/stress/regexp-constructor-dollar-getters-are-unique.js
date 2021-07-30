function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

const seenGetters = new WeakSet();

for (const [key, desc] of Object.entries(Object.getOwnPropertyDescriptors(RegExp))) {
    if (!/^\$\d$/.test(key))
        continue;

    assert(typeof desc.get === "function");
    assert(!seenGetters.has(desc.get));

    seenGetters.add(desc.get);
}
