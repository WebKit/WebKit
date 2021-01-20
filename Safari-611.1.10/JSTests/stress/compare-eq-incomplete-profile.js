var createBuiltin = $vm.createBuiltin;

const test = createBuiltin(`(function (arg) {
    let other = @undefined;
    @idWithProfile(other, "SpecObject");
    return arg == other;
})`);

for (let i = 0; i < 10000; i++) {
    test({});
    test(null);
}
