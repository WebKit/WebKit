const testGetterSetter = $vm.createCustomTestGetterSetter();
Reflect.set({}, 'customValue', 'foo', testGetterSetter);
testGetterSetter.customValue = 42;
