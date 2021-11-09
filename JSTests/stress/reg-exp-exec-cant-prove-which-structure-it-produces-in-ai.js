// This should not crash.

function foo() {
    let r = /a/;
    r.compile(undefined, ...'d');
    let a = r.exec(/b/);
    a.x;
}

for (let i = 0; i < 1000; i++) {
    foo();
}
