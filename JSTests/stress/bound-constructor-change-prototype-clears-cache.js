function empty() {

}

function test1() {
    const newTarget = (function () {}).bind();
    newTarget.prototype = Object.prototype;

    const a = Reflect.construct(Promise, [empty], newTarget);

    newTarget.prototype = Array.prototype;

    const b = Reflect.construct(Promise, [empty], newTarget);

    if (a.__proto__ === b.__proto__)
        throw new Error('They should be different.');
}

function test2() {
    const newTarget = (function () {}).bind();
    newTarget.prototype = Object.prototype;

    const newTargetWrapper = Function.prototype.apply;
    newTargetWrapper.prototype = newTarget;

    class Opt extends Promise {
        constructor() {
            newTargetWrapper.prototype = new.target;
            empty instanceof newTargetWrapper;
            
            super(empty);
        }
    }

    for (let i = 0; i < 200000; i++) {
        Reflect.construct(Opt, [], newTarget);
    }

    const a = Reflect.construct(Opt, [], newTarget);

    newTarget.prototype = Array.prototype;
    Reflect.construct(Object, [], newTarget);

    const b = Reflect.construct(Opt, [], newTarget);

    if (a.__proto__ === b.__proto__)
        throw new Error('They should be different.');
}

function test3() {
    const newTarget = (function () {}).bind();
    let prototype = Object.prototype;
    Object.defineProperty(newTarget, "prototype", { get() { return prototype; }});

    const a = Reflect.construct(Promise, [empty], newTarget);

    prototype = Array.prototype;

    const b = Reflect.construct(Promise, [empty], newTarget);

    if (a.__proto__ == b.__proto__)
        throw new Error('They should be different.');
}

function main() {
    test1();

    test2();

    test3();
}

main();
