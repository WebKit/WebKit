function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error('bad error: ' + error);
}

let key = 'tag';

class A {
    tag() {
        return this;
    }
    static tag() {
        return this;
    }
    get getter() {
        let receiver = this;
        return function () {
            return [receiver, this];
        };
    }
    mark() {
        this.marked = true;
    }
}

class B extends A {
    dot() {
        return super.tag`x`;
    }
    bracket() {
        return super['tag']`x`;
    }
    computed() {
        return super[key]`x`;
    }
    parenthesized() {
        return (super.tag)`x`;
    }
    withExpressions() {
        return super.tag`a${1}b${2}c`;
    }
    arrow() {
        return (() => super.tag`x`)();
    }
    viaEval() {
        return eval('super.tag`x`');
    }
    getterDot() {
        return super.getter`x`;
    }
    getterBracket() {
        return super['getter']`x`;
    }
    notTail() {
        let result = super.tag`x`;
        return result;
    }
    markSelf() {
        super.mark`x`;
    }
    static dot() {
        return super.tag`x`;
    }
    static bracket() {
        return super[key]`x`;
    }
}

class C extends A {
    constructor(beforeSuper) {
        if (beforeSuper)
            super.tag`x`;
        super();
        this.dot = super.tag`x`;
        this.bracket = super[key]`x`;
        this.arrow = (() => super.tag`x`)();
    }
}

let proto = {
    tag() {
        'use strict';
        return this;
    }
};

let object = {
    __proto__: proto,
    dot() {
        return super.tag`x`;
    },
    bracket() {
        return super[key]`x`;
    }
};

function test() {
    let b = new B;
    shouldBe(b.dot(), b);
    shouldBe(b.bracket(), b);
    shouldBe(b.computed(), b);
    shouldBe(b.parenthesized(), b);
    shouldBe(b.withExpressions(), b);
    shouldBe(b.arrow(), b);
    shouldBe(b.notTail(), b);

    let [receiver, thisValue] = b.getterDot();
    shouldBe(receiver, b);
    shouldBe(thisValue, b);
    [receiver, thisValue] = b.getterBracket();
    shouldBe(receiver, b);
    shouldBe(thisValue, b);

    b.markSelf();
    shouldBe(b.hasOwnProperty('marked'), true);
    shouldBe(A.prototype.hasOwnProperty('marked'), false);

    shouldBe(B.dot(), B);
    shouldBe(B.bracket(), B);

    shouldBe(B.prototype.dot.call(1), 1);
    shouldBe(B.prototype.bracket.call(undefined), undefined);

    let c = new C(false);
    shouldBe(c.dot, c);
    shouldBe(c.bracket, c);
    shouldBe(c.arrow, c);

    shouldBe(object.dot(), object);
    shouldBe(object.bracket(), object);
}

for (let i = 0; i < testLoopCount; i++)
    test();

let b = new B;
shouldBe(b.viaEval(), b);
shouldThrow(() => new C(true), ReferenceError);
