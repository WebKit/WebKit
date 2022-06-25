var createBuiltin = $vm.createBuiltin;

var target = createBuiltin(`(function (array)
{
    if (array) {
        @idWithProfile(array, "SpecOther").push(42);
    }
    return array;
})`);
noInline(target);
for (var i = 0; i < 1e5; ++i)
    target([42]);
