var createBuiltin = $vm.createBuiltin;

const test = createBuiltin(`(function (arg) {
    let other = @undefined;
    @idWithProfile(other, "SpecObject");
    return arg == other;
})`);

for (let i = 0; i < testLoopCount; i++) {
    test({});
    test(null);
}
