//@ runNoFTL; runNoLLInt; runFTLNoCJITValidate

"use strict";

var verbose = false;
load("resources/shadow-chicken-support.js", "caller relative");
initialize();

(function test1() {
    function foo() {
        expectStack([foo, test1]);
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
    function foo() {
        expectStack([foo, test3]);
    }
    
    function bar() {
        return foo();
    }

    function baz() {
        return bar();
    }
    
    baz();
})();


