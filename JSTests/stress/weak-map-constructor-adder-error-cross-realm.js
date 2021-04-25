const realmA = $vm.createGlobalObject();
const realmB = $vm.createGlobalObject();
const realmC = $vm.createGlobalObject();

realmB.WeakMap.prototype.set =
realmC.WeakMap.prototype.set;

try {
    Reflect.construct(realmA.WeakMap, [[[0, 0]]], realmB.WeakMap);
    throw "Didn't throw!";
} catch (err) {
    if (!(err instanceof realmC.TypeError))
        throw "Bad realm of an error!";
}
