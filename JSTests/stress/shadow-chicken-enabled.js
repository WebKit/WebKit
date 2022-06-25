//@ runShadowChicken

"use strict";

var shadowChickenFunctionsOnStack = $vm.shadowChickenFunctionsOnStack;

var verbose = false;

load("resources/shadow-chicken-support.js", "caller relative");
initialize();

(function test1() {
    function foo() {
        expectStack([foo, bar, baz, test1]);
    }
    
    function bar() {
        return foo();
    }

    function baz() {
        return bar();
    }
    
    baz();
})();

(function test2() {
    function foo() {
    }
    
    function bar() {
        return foo();
    }

    function baz() {
        return bar();
    }
    
    baz();
})();

(function test3() {
    if (verbose) {
        print("test3:");
        print("bob = " + describe(bob));
        print("thingy = " + describe(thingy));
        print("foo = " + describe(foo));
        print("bar = " + describe(bar));
        print("baz = " + describe(baz));
    }
    
    function bob() {
        if (verbose)
            print("Doing bob...");
        expectStack([bob, thingy, foo, bar, baz, test3]);
    }
    
    function thingy() {
        return bob();
    }
    
    function foo() {
        if (verbose)
            print("Doing foo...");
        expectStack([foo, bar, baz, test3]);
        return thingy();
    }
    
    function bar() {
        return foo();
    }

    function baz() {
        return bar();
    }
    
    baz();
})();

(function test4() {
    if (verbose) {
        print("test4:");
        print("bob = " + describe(bob));
        print("thingy = " + describe(thingy));
        print("foo = " + describe(foo));
        print("bar = " + describe(bar));
        print("baz = " + describe(baz));
    }
    
    function bob(thingyIsTail) {
        if (verbose)
            print("Doing bob...");
        expectStack([bob, thingy, foo, bar, baz, test4]);
    }
    
    function thingy(isTail) {
        bob(false);
        return bob(isTail);
    }
    
    function foo() {
        if (verbose)
            print("Doing foo...");
        expectStack([foo, bar, baz, test4]);
        thingy(false);
        return thingy(true);
    }
    
    function bar() {
        foo();
        return foo();
    }

    function baz() {
        bar();
        return bar();
    }
    
    baz();
})();

(function test5a() {
    if (verbose)
        print("In test5a:");
    var foos = 50;
    
    function foo(ttl) {
        if (ttl <= 1) {
            var array = [];
            for (var i = 0; i < foos; ++i)
                array.push(foo);
            array.push(test5a);
            expectStack(array);
            return;
        }
        return foo(ttl - 1);
    }
    
    foo(foos);
})();

(function test5b() {
    if (verbose)
        print("In test5b:");
    var foos = 100;
    
    function foo(ttl) {
        if (ttl <= 1) {
            var array = [];
            for (var i = 0; i < foos; ++i)
                array.push(foo);
            array.push(test5b);
            expectStack(array);
            return;
        }
        return foo(ttl - 1);
    }
    
    foo(foos);
})();

(function test6() {
    if (verbose) {
        print("In test6");
        print("foo = " + describe(foo));
        print("array.push = " + describe([].push));
    }
    
    var foos = 128;
    
    function foo(ttl) {
        if (ttl <= 1) {
            var array = [];
            for (var i = 0; i < foos; ++i)
                array.push(foo);
            array.push(test6);
            expectStack(array);
            return;
        }
        return foo(ttl - 1);
    }
    
    foo(foos);
    
    if (verbose)
        print("Done with test6.");
})();

(function test7() {
    var foos = 200000;
    
    function foo(ttl) {
        if (ttl <= 1) {
            var stack = shadowChickenFunctionsOnStack();
            var expectedStack = [];
            expectedStack.push(shadowChickenFunctionsOnStack);
            while (expectedStack.length < stack.length - 2)
                expectedStack.push(foo);
            expectedStack.push(test7);
            expectedStack.push(stackTop);
            compareStacks(stack, expectedStack);
            return;
        }
        return foo(ttl - 1);
    }
    
    foo(foos);
})();

