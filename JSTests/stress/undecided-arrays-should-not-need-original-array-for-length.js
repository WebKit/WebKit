//@ $skipModes << :lockdown if $buildType == "debug"

function findFoo(name, array, bail) {
    let some = array.some
    if (bail)
        return;
    some.call(array, function (v) {
        return v === name;
    });
}
noInline(findFoo);

class SubArray extends Array {}
for (let i = 0; i < 1e5; ++i) {
    let array = i % 2 ? new SubArray(0) : ["foo"];
    findFoo("foo", array, array.length);
    for (let i = 0; i < 100; ++i)
        [].some(function (v) { });
}
