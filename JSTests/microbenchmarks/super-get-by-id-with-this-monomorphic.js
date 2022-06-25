//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
const run = new Function("init", "num", `
const calc = val => {
    let c = 0;
    for (let v = val; v; v >>>= 1) c += v & 1;
    return val * 2 + val / 2 + c;
}

class A {
    constructor(x) { this._value = x; }
    set value(x) { this._value = x; }
    get value() { return this._value; }
}
class B extends A {
    set value(x) { super.value = x; }
    get value() { return calc(super.value); }
}

const bench = (init, num) => {
    let arr = [];
    for (let i = 0; i != num; ++i) arr.push(new B(init));
    for (let i = 0; i != num; ++i) arr[i].value += i;
    let sum = 0;
    for (let i = 0; i != num; ++i) sum += arr[i].value;
};

bench(init, num);
`);

run(2, 10000);
run(1 << 30, 10000);
run(42.2, 10000);
run(42.5e10, 10000);
