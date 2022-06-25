(function() {
    var proxy = $vm.createProxy({ set foo(_v) {} });

    for (var j = 0; j < 1e5; j++)
        proxy["foo" + j] = j;
})();
