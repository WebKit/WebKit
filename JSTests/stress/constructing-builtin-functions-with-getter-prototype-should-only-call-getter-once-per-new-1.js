var invokeCount = 0;

Object.defineProperty(Function.prototype, 'prototype', {
    get: function () {
        invokeCount++;
    }
});

new Promise(resolve => {
    for (var i = 0; i < 10000; ++i)
        new resolve();

    if (invokeCount != 10000)
        $vm.crash();
});
