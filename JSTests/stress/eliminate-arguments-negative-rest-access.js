//@ requireOptions("--forceEagerCompilation=1")

function inlinee(index, value, ...rest) {
    return rest[index | 0];
}

function opt() {
    return inlinee(-1, 0x1234);
}
noInline(opt);

for (let i = 0; i < 1e6; i++) {
    const value = opt();
    if (value !== undefined)
        throw new Error(`${i}: ${value}`);
}
