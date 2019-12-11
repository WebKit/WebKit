const a = [0];

function foo() {
    for (const x1 of a) {
        for (const x2 of a) {
            with (0) {
                Object;
            }
        }
    }
    for (let i = 0; i < 100; i++) {
    }
}
foo();
