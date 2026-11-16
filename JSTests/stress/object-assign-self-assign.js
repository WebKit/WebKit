function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

for (let i = 0; i < 5; i++) {
    function CLS(value) { this.prop = value; }
    const a = new CLS(5);
    const b = new CLS(6);
    Object.assign(a, b, a);
    sameValue(a.prop, 6);
}
