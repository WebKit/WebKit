new Promise(resolve => {
    var invokeCount = 0;

    Object.defineProperty(resolve, 'prototype', {
        get: function () {
            invokeCount++;
        }
    });

    for (var i = 0; i < testLoopCount; ++i)
        new resolve();

    if (invokeCount != 10000)
        $vm.crash();
});
