function foo() {
    let s = new Uint8Array().toLocaleString();
    let s2 = s + 'x';
    s2[s2] = 0;
}

noInline(foo);
for (let i = 0; i < 1e6; i++) {
    foo();
}
