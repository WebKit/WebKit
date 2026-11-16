function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const realmA = createGlobalObject();
const realmB = createGlobalObject();

const executor = function() {};
const newTarget = new realmA.Function();
newTarget.prototype = null;

for (let i = 0; i < 1e4; ++i) {
    let promise = Reflect.construct(realmB.Promise, [executor], newTarget);
    shouldBe(Object.getPrototypeOf(promise), realmA.Promise.prototype);
}
