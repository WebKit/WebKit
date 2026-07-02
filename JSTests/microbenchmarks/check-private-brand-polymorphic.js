function makeClasses(n) {
    const classes = [];
    for (let i = 0; i < n; i++)
        classes.push(class { #m() { return i; } go() { return this.#m() + this.#m(); } });
    return classes;
}

const objs = makeClasses(8).map(C => new C());

function test(objs) {
    let sum = 0;
    for (let j = 0; j < objs.length; j++)
        sum += objs[j].go();
    return sum;
}
noInline(test);

for (let i = 0; i < 1e6; ++i)
    test(objs);
