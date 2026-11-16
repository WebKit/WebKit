const a = [];
function bar(a0) {
    let j = 0;
    while (j++ < 1000) {
        a.splice(a0, 1000, 0, {}, {}, {}, {});
    }
}

function foo() {
    for (let k = 0; k < 10; k++) {
        bar(2**25);
    }
    bar();
}

for (let i = 0; i < 30; i++) {
    foo();
}
