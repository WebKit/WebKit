const realmA = $vm.createGlobalObject();
const realmB = $vm.createGlobalObject();
const realmC = $vm.createGlobalObject();

realmB.WeakSet.prototype.add =
realmC.WeakSet.prototype.add;

try {
    Reflect.construct(realmA.WeakSet, [[0]], realmB.WeakSet);
    throw "Didn't throw!";
} catch (err) {
    if (!(err instanceof realmC.TypeError))
        throw "Bad realm of an error!";
}
