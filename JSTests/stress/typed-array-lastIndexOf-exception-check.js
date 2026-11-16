//@ runDefault

new Uint8Array().lastIndexOf(0, {
    valueOf: () => -1
});
