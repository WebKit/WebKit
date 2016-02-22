(function(){
    "use strict";
    var it = [][Symbol.iterator]();
    while (it) {
        if (it.hasOwnProperty('next'))
            delete it.next;
        it = Object.getPrototypeOf(it);
    }

    var bind = Function.prototype.bind;
    var uncurryThis = bind.bind(bind.call);

    var bindFn = uncurryThis(bind);
    var applyFn = uncurryThis(bind.apply);
    function test() { print("here"); }
    var sliceFn = uncurryThis([].slice);
    function addAll(var_args) {
        var args = sliceFn(arguments, 0);
        var result = this;
        for (var i = 0; i < args.length; i++)
            result += args[i];
        return result;
    }
    var sum;

    try {
        sum = applyFn(addAll, 3, [4, 5, 6]);
    } catch (err) {
        print('oops ', err);
    }
    print('sum ', sum);
})();
