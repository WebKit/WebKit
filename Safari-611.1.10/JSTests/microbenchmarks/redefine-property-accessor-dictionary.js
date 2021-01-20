//@ skip if $memoryLimited and ["arm", "mips"].include?($architecture)
const obj = {};
for (let i = 0; i < 100; ++i)
    obj["k" + i] = i;

function getter1() {}
function getter2() {}
function setter1(v) {}
function setter2(v) {}

const descGet1 = {get: getter1, configurable: true};
const descGet1Set1 = {get: getter1, set: setter1};
const descGet2Set1 = {get: getter2, set: setter1};
const descGet1Set2 = {get: getter1, set: setter2};
const descGet2Set2 = {get: getter2, set: setter2};

for (let i = 0; i < 1e4; ++i) {
    const key = "k" + (i % 100);
    Object.defineProperty(obj, key, descGet1);
    Object.defineProperty(obj, key, descGet1Set1);
    Object.defineProperty(obj, key, descGet2Set1);
    Object.defineProperty(obj, key, descGet1Set2);
    Object.defineProperty(obj, key, descGet2Set2);
}
