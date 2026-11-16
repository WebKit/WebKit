function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function Node(next) {
    this.next = next;
    this.value = next ? next.value + 1 : 0;
    this.self = this.next;
}

Node.prototype.update = function (other) {
    this.value = other.value;
    this.next = this.self;
    this[this.value] = this.next;
    this.flag = this === other.self;
    let holder = {};
    let tmp;
    holder[this] = (tmp = other, tmp.value + 1);
    this.keyed = holder["node" + this.value];
    return this;
};

Node.prototype.toString = function () {
    return "node" + this.value;
};

Node.prototype.compare = function (other) {
    let tmp;
    let results = [this === (tmp = other), this == (tmp = this), this instanceof (tmp = Node), this in (tmp = { node0: 1 })];
    this.value += (tmp = 2);
    this.fresh ||= (tmp = 3);
    this[(tmp = "keyed")] ??= (tmp = 4);
    results.push(this.value, this.fresh, this.keyed);
    return results.join();
};

function* generator() {
    this.x = yield 1;
    this.y = this.x + (yield 2);
}

async function asyncFunction(value) {
    this.x = await value;
    this[await "y"] = await value + 1;
}

async function* asyncGenerator(value) {
    this.x = await value;
    this.y = yield this.x;
}

class Base {
    constructor() {
        this.n = 1;
        this.tag = "tag";
        ++Base.count;
    }
}
Base.count = 0;

class Derived extends Base {
    constructor(results) {
        super();
        let self = this;
        (async () => {
            this.x = await 2;
            this[await "n"]++;
            results.push(this[await "tag"], this === await self, this instanceof await Derived, this.x, this.n);
        })().catch((error) => results.push(error));
    }
}

class DerivedCallingSuperAfterAwait extends Base {
    constructor(results) {
        (async () => {
            await 0;
            super();
            this.x = await 3;
            results.push(this.x, this[await "n"]);
        })().catch((error) => results.push(error));
        return {};
    }
}

class DerivedCallingSuperAfterAwaitThenComparing extends Base {
    constructor(results) {
        (async () => {
            await 0;
            super();
            let self = this;
            results.push(this === await self, this instanceof await Base);
        })().catch((error) => results.push(error));
        return {};
    }
}

class DerivedNeverCallingSuper extends Base {
    constructor(results) {
        let pushError = (error) => results.push(error.constructor.name);
        (async () => { await 0; return this.n; })().then(() => results.push("resolved"), pushError);
        (async () => { await 0; this.x = await 1; })().then(() => results.push("resolved"), pushError);
        (async () => { await 0; return super.constructor; })().then(() => results.push("resolved"), pushError);
        (async () => { await 0; return eval("this"); })().then(() => results.push("resolved"), pushError);
        return {};
    }
}

class DerivedWithNestedAsyncArrow extends Base {
    constructor(results) {
        super();
        let self = this;
        (() => {
            (async (value = this) => {
                results.push(value === self, this === await self);
            })().catch((error) => results.push(error));
        })();
    }
}

class KeyedBase {
    constructor() {
        this.n = 1;
        this.s = "s";
    }

    toString() {
        return "key";
    }
}

class DerivedHoldingThisAcrossAwait extends KeyedBase {
    constructor(results) {
        super();
        let self = this;
        (async () => {
            results.push(this == await self, this !== await self, this in await { key: 1 });
            this.n += await 1;
            this[await "n"] *= await 3;
            this.s ||= await "unused";
            this.u ??= await "u";
            this[await "v"] ??= await "v";
            let holder = {};
            holder[this] = await 4;
            holder[this] += await 1;
            [this.a, this[await "b"]] = [await 6, await 7];
            ({ c: this.c } = await { c: 8 });
            for (this.d of [await 9]) { }
            for await (this.e of [10]) { }
            results.push(this.n, this.s, this.u, this.v, holder.key, this.a, this.b, this.c, this.d, this.e);
        })().catch((error) => results.push(error));
    }
}

class DerivedAssigningEvalResult extends Base {
    constructor() {
        let arrow = () => this;
        super();
        let self = this;
        this.x = eval("1");
        this[eval("'y'")] = (eval("2"), arrow() === self);
        let threw = false;
        try {
            this.z = eval("super(), 3");
        } catch (error) {
            threw = error instanceof ReferenceError;
        }
        shouldBe(threw, true);
        shouldBe(this, self);
        shouldBe(self.x, 1);
        shouldBe(self.y, true);
        shouldBe(self.z, undefined);
    }
}

class DerivedCallingSuperInRightHandSide extends Base {
    constructor() {
        super();
        let first = this;
        let callSuper = () => {
            try {
                super();
            } catch (error) {
                shouldBe(error instanceof ReferenceError, true);
            }
            return 4;
        };
        this.x = callSuper();
        shouldBe(this, first);
        shouldBe(first.x, 4);
    }
}

class DerivedCallingSuperBeforeBinding extends Base {
    constructor() {
        let callSuper = () => {
            super();
            return 5;
        };
        try {
            this.x = callSuper();
        } catch (error) {
            shouldBe(error instanceof ReferenceError, true);
        }
        super();
        shouldBe(this.x, undefined);
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    let a = new Node(null);
    let b = new Node(a);
    shouldBe(b.value, 1);
    shouldBe(b.self, a);
    shouldBe(b.update(a), b);
    shouldBe(b.value, 0);
    shouldBe(b.next, a);
    shouldBe(b[0], a);
    shouldBe(b.flag, false);
    shouldBe(b.keyed, 1);
    let c = new Node(null);
    shouldBe(c.compare(c), "true,true,true,true,2,3,4");

    let generatorThis = {};
    let iterator = generator.call(generatorThis);
    iterator.next();
    iterator.next(10);
    iterator.next(20);
    shouldBe(generatorThis.x, 10);
    shouldBe(generatorThis.y, 30);

    let asyncThis = {};
    asyncFunction.call(asyncThis, 6);
    let asyncGeneratorThis = {};
    let asyncIterator = asyncGenerator.call(asyncGeneratorThis, 7);
    asyncIterator.next();
    let asyncArrowThis = {};
    (function () {
        (async () => {
            this.x = await 8;
            this.y = this === await asyncArrowThis;
        })();
    }).call(asyncArrowThis);

    let derivedResults = [];
    new Derived(derivedResults);
    let superAfterAwaitResults = [];
    new DerivedCallingSuperAfterAwait(superAfterAwaitResults);
    Base.count = 0;
    new DerivedCallingSuperInRightHandSide();
    shouldBe(Base.count, 2);
    new DerivedCallingSuperBeforeBinding();
    let superAfterAwaitComparingResults = [];
    new DerivedCallingSuperAfterAwaitThenComparing(superAfterAwaitComparingResults);
    let neverCallingSuperResults = [];
    new DerivedNeverCallingSuper(neverCallingSuperResults);
    let nestedResults = [];
    new DerivedWithNestedAsyncArrow(nestedResults);
    let holdingResults = [];
    new DerivedHoldingThisAcrossAwait(holdingResults);
    new DerivedAssigningEvalResult();

    drainMicrotasks();
    asyncIterator.next(9);
    drainMicrotasks();

    shouldBe(asyncThis.x, 6);
    shouldBe(asyncThis.y, 7);
    shouldBe(asyncGeneratorThis.x, 7);
    shouldBe(asyncGeneratorThis.y, 9);
    shouldBe(asyncArrowThis.x, 8);
    shouldBe(asyncArrowThis.y, true);
    shouldBe(derivedResults.join(), "tag,true,true,2,2");
    shouldBe(superAfterAwaitResults.join(), "3,1");
    shouldBe(superAfterAwaitComparingResults.join(), "true,true");
    shouldBe(neverCallingSuperResults.join(), "ReferenceError,ReferenceError,ReferenceError,ReferenceError");
    shouldBe(nestedResults.join(), "true,true");
    shouldBe(holdingResults.join(), "true,false,true,6,s,u,v,5,6,7,8,9,10");
}
