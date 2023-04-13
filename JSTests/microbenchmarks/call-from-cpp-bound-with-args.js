function test(a, b, c) { }
$vm.callFromCPP(test.bind(null, 0, 1, 2), 4e6);
