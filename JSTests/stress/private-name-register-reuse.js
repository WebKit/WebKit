function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function join(...args) {
    return args.join(",");
}
noInline(join);

class Point {
    #x;
    #y;
    #z;
    static #count = 0;

    constructor(x, y, z) {
        this.#x = x;
        this.#y = y;
        this.#z = z;
        Point.#count = Point.#count + 1;
    }

    static count() { return Point.#count; }
    static sub(v, w) { return new Point(v.#x - w.#x, v.#y - w.#y, v.#z - w.#z); }
    #method() { return this.#x + this.#y; }
    get #accessor() { return this.#z; }

    dot(w) { return this.#x * w.#x + this.#y * w.#y + this.#z * w.#z; }
    array() { return [this.#x, this.#y, this.#x, this.#z, this.#y]; }
    template() { return `${this.#x}:${this.#y}:${this.#x}:${this.#z}`; }
    concat() { return "x" + this.#x + "y" + this.#y + "x" + this.#x; }
    call() { return join(this.#x, this.#y, this.#x); }
    nestedCall() { return join(this.#x, join(this.#y, this.#x), this.#x) + this.#x; }
    acrossCall() {
        let a = this.#x;
        let b = join(this.#y, a);
        return a + b + this.#x + this.#y;
    }
    spread() { return join(...[this.#x, this.#y], this.#x); }
    construct() { return new Array(this.#x, this.#y, this.#x).join(","); }
    conditional(c) { return (c ? this.#x : this.#y) + this.#x; }
    branch(c) {
        if (c)
            this.#y;
        else
            this.#x;
        return this.#x * 10 + this.#y;
    }
    logical(o) { return (o && o.#x) + this.#x; }
    optional(o) { return join(o?.#x, this.#x); }
    loop(n) {
        let sum = this.#y;
        for (let i = 0; i < n; ++i) {
            if (i & 1)
                continue;
            sum += this.#x + this.#y;
        }
        return sum + this.#x;
    }
    labeled(n) {
        let sum = 0;
        outer: for (let i = 0; i < n; ++i) {
            for (let j = 0; j < n; ++j) {
                if (j > i)
                    continue outer;
                sum += this.#x;
            }
            sum += this.#y;
        }
        return sum + this.#x + this.#y;
    }
    select(k) {
        switch (k) {
        case 0:
            return this.#x;
        case 1:
            k = this.#y;
        case 2:
            return k + this.#x + this.#y;
        default:
            return this.#z;
        }
    }
    tryCatch(o) {
        try {
            return this.#x + o.#x;
        } catch {
            return this.#x + this.#y;
        }
    }
    tryFinally() {
        let result = 0;
        try {
            result = this.#x;
        } finally {
            result += this.#y + this.#x;
        }
        return result + this.#y;
    }
    has(o) { return (#x in o) + (#x in this) + (#x in o ? o.#x + this.#x : this.#x); }
    write(v) {
        this.#x = v;
        this.#y = this.#x + v;
        this.#x += this.#y;
        this.#x++;
        ++this.#y;
        this.#z ??= this.#x;
        return join(this.#x--, --this.#y, this.#x, this.#y, this.#z);
    }
    swap() {
        [this.#x, this.#y] = [this.#y, this.#x];
        ({ a: this.#z, b: this.#x } = { a: this.#x, b: this.#z });
        return join(this.#x, this.#y, this.#z);
    }
    mixed() { return this.#method() + this.#x + this.#accessor + this.#method() + this.#z; }
    *generator() {
        yield this.#x;
        yield this.#x + this.#y;
        return this.#x;
    }
    async asynchronous() {
        let a = this.#x;
        await null;
        return a + this.#x + this.#y;
    }
    arrow() { return [1, 2].map((i) => this.#x * i + this.#x).join(","); }
    defaultParameter(a = this.#x, b = this.#x + a) { return a + b + this.#x; }
    evaluate() { return eval("this.#x + this.#x") + this.#x; }
    shadow() {
        let result = 0;
        try {
            result = this.#x;
            class Inner {
                #x;
                static [this.#x] = 0;
            }
            result = -1;
        } catch (error) {
            if (!(error instanceof TypeError))
                throw error;
        }
        return result + this.#x;
    }
}

class Many {
    #f0 = 0; #f1 = 1; #f2 = 2; #f3 = 3; #f4 = 4; #f5 = 5; #f6 = 6; #f7 = 7; #f8 = 8;
    #f9 = 9; #f10 = 10; #f11 = 11; #f12 = 12; #f13 = 13; #f14 = 14; #f15 = 15; #f16 = 16;
    #sum = this.#f0 + this.#f16 + this.#f0;

    sum() {
        return this.#f0 + this.#f1 + this.#f2 + this.#f3 + this.#f4 + this.#f5 + this.#f6 + this.#f7 + this.#f8
            + this.#f9 + this.#f10 + this.#f11 + this.#f12 + this.#f13 + this.#f14 + this.#f15 + this.#f16
            + this.#f16 + this.#f15 + this.#f14 + this.#f13 + this.#f12 + this.#f11 + this.#f10 + this.#f9
            + this.#f8 + this.#f7 + this.#f6 + this.#f5 + this.#f4 + this.#f3 + this.#f2 + this.#f1 + this.#f0 + this.#sum;
    }
    array() {
        return [this.#f0, this.#f1, this.#f2, this.#f3, this.#f4, this.#f5, this.#f6, this.#f7, this.#f8,
            this.#f9, this.#f10, this.#f11, this.#f12, this.#f13, this.#f14, this.#f15, this.#f16,
            this.#f15, this.#f16, this.#f0].join("");
    }
}

let p = new Point(1, 2, 3);
let q = new Point(4, 5, 6);
let many = new Many;

function testStraightLine() {
    shouldBe(Point.sub(q, p).array().join(","), "3,3,3,3,3");
    shouldBe(p.dot(q), 32);
    shouldBe(p.array().join(","), "1,2,1,3,2");
    shouldBe(p.template(), "1:2:1:3");
    shouldBe(p.concat(), "x1y2x1");
    shouldBe(p.mixed(), 13);
    shouldBe(many.sum(), 288);
    shouldBe(many.array(), "01234567891011121314151615160");
}

function testCalls() {
    shouldBe(p.call(), "1,2,1");
    shouldBe(p.nestedCall(), "1,2,1,11");
    shouldBe(p.acrossCall(), "12,112");
    shouldBe(p.spread(), "1,2,1");
    shouldBe(p.construct(), "1,2,1");
    shouldBe(p.arrow(), "2,3");
    shouldBe(p.defaultParameter(), 4);
}

function testControlFlow(i) {
    let odd = !!(i & 1);
    shouldBe(p.conditional(odd), odd ? 2 : 3);
    shouldBe(p.branch(odd), 12);
    shouldBe(p.logical(odd ? q : 0), odd ? 5 : 1);
    shouldBe(p.optional(odd ? q : null), odd ? "4,1" : ",1");
    shouldBe(p.loop(4), 9);
    shouldBe(p.labeled(3), 11);
    shouldBe(p.select(i % 4), [1, 5, 5, 3][i % 4]);
    shouldBe(p.tryCatch(odd ? q : {}), odd ? 5 : 3);
    shouldBe(p.tryFinally(), 6);
    shouldBe(p.has(odd ? q : {}), odd ? 7 : 2);
}

function testWrites() {
    let r = new Point(4, 5, 6);
    shouldBe(r.write(7), "22,14,21,14,6");
    shouldBe(r.swap(), "6,21,14");
}

for (let i = 0; i < testLoopCount; ++i) {
    testStraightLine();
    testCalls();
    testControlFlow(i);
    testWrites();
}

for (let i = 0; i < 2; ++i) {
    shouldBe(p.shadow(), 2);
    shouldBe(p.evaluate(), 3);

    let generator = p.generator();
    shouldBe(generator.next().value, 1);
    shouldBe(generator.next().value, 3);
    shouldBe(generator.next().value, 1);

    let result;
    p.asynchronous().then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(result, 4);
}

shouldBe(Point.count(), testLoopCount * 2 + 2);
