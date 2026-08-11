function foo() {
    for (let x in []) {
        x in undefined;
        x = 0;
        [][x];
    }
}
foo();

