function foo() {
    '' + '' + 1n;
}

for (let i = 0; i < 100; i++) {
    foo();
}
