function foo() {
    try {
        foo();
    } catch {
        SubtleCrypto.prototype.unwrapKey();
    }
}
foo();
