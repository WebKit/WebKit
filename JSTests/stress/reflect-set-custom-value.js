{
    // reified
    const testGetterSetter = $vm.createCustomTestGetterSetter();
    Reflect.set({}, 'customValue', 'foo', testGetterSetter);
    testGetterSetter.customValue = 42;
}

{
    // non-reified
    let tester = $vm.createStaticCustomValue();
    Reflect.set({}, "testStaticValueSetFlag", 'foo', tester);
    if (!tester.testStaticValueSetterCalled)
        throw new Error('Custom value overriden');
}
