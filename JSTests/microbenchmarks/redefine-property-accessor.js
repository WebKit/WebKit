function getter1() {}
function getter2() {}
function setter1(v) {}
function setter2(v) {}

const obj = {a: 0, b: 1, get foo() {}, c: 3, d: 4};
const descGet1 = {get: getter1, configurable: true};
const descGet1Set1 = {get: getter1, set: setter1};
const descGet2Set1 = {get: getter2, set: setter1};
const descGet1Set2 = {get: getter1, set: setter2};
const descGet2Set2 = {get: getter2, set: setter2};

for (let i = 0; i < 1e4; ++i) {
    Object.defineProperty(obj, "foo", descGet1);
    Object.defineProperty(obj, "foo", descGet1Set1);
    Object.defineProperty(obj, "foo", descGet2Set1);
    Object.defineProperty(obj, "foo", descGet1Set2);
    Object.defineProperty(obj, "foo", descGet2Set2);
}
