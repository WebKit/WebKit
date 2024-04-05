function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const re = /./;
let count = 0;
Object.defineProperty(re, 'flags', {
    get() {
        count++;
        return '';
    }
});
for (let i = 0; i < 1e3; i++) {
    re[Symbol.match]('');
}
shouldBe(count, 1e3);
