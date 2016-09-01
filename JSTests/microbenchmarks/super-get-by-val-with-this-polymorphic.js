function value() { return 'v'; }
noInline(value);

const calc = val => {
    let c = 0;
    for (let v = val; v; v >>>= 1) c += v & 1;
    return val * 2 + val / 2 + c;
}

class A {
    constructor(x) { this._v = x; }
    set v(x) { this._v = x; }
    get v() { return this._v; }
}
class B extends A {
    set v(x) { super[value()] = x; }
    get v() { return calc(super[value()]); }
}

const bench = (init, num) => {
    let arr = [];
    for (let i = 0; i != num; ++i) arr.push(new B(init));
    for (let i = 0; i != num; ++i) arr[i].v += i;
    let sum = 0;
    for (let i = 0; i != num; ++i) sum += arr[i].v;
};

bench(2, 10000);
bench(1 << 30, 10000);
bench(42.2, 10000);
bench(42.5e10, 10000);
