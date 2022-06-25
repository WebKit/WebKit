const x = 0;

function foo() {
    for (const q in 0) { }
}

for (let i = 0; i < 5; i++) {
    foo();
    Number.prototype.valueOf = 0;
}
