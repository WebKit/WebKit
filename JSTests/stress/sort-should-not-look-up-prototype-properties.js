// This should not crash.

function assert(b) {
    if (!b)
        throw new Error;
}

x = Array.prototype;

for (let i = 0; i < 100; i++) {
    x[50] = 1;
    x.sort();
    if (i <= 50)
        assert(x[i] === 1);
}
