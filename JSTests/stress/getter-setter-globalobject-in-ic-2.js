function assert(b) {
    if (!b)
        throw new Error("Bad assertion!");
}

const createCustom = `
    var custom = $vm.createCustomTestGetterSetter();
`;

(function directCustomAccessorGet() {
    const other = runString(createCustom);
    const otherCustom = other.custom;

    for (let i = 0; i < 1e5; i++) {
        assert(otherCustom.customAccessorGlobalObject === other);
    }
})();

(function directCustomValueGet() {
    const other = runString(createCustom);
    const otherCustom = other.custom;

    for (let i = 0; i < 1e5; i++) {
        assert(otherCustom.customValueGlobalObject === other);
    }
})();

(function directCustomAccessorSet() {
    const other = runString(createCustom);
    const otherCustom = other.custom;

    for (let i = 0; i < 1e5; i++) {
        const value = {};
        otherCustom.customAccessorGlobalObject = value;
        assert(value.result === other);
    }
})();

(function directCustomValueSet() {
    const other = runString(createCustom);
    const otherCustom = other.custom;

    for (let i = 0; i < 1e5; i++) {
        const value = {};
        otherCustom.customValueGlobalObject = value;
        assert(value.result === other);
    }
})();

(function prototypeChainCustomAccessorGet() {
    const other = runString(createCustom);
    const otherCustomHeir = Object.create(other.custom);

    for (let i = 0; i < 1e5; i++) {
        assert(otherCustomHeir.customAccessorGlobalObject === other);
    }
})();

(function prototypeChainCustomValueGet() {
    const other = runString(createCustom);
    const otherCustomHeir = Object.create(other.custom);

    for (let i = 0; i < 1e5; i++) {
        assert(otherCustomHeir.customValueGlobalObject === other);
    }
})();

(function prototypeChainCustomAccessorSet() {
    const other = runString(createCustom);
    const otherCustomHeir = Object.create(other.custom);

    for (let i = 0; i < 1e5; i++) {
        const value = {};
        otherCustomHeir.customAccessorGlobalObject = value;
        assert(value.result === other);
    }
})();
