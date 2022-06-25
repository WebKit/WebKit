//@ runDefault

new BigInt64Array().lastIndexOf(0n, {
    valueOf: () => -1
});
