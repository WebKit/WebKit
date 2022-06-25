function test() {

// Note: only available outside of strict mode.
if (!this) return false;

if(true) function foo() { return 2; }
if(false) {} else function bar() { return 3; }
if(true) function baz() { return 4; } else {}
if(false) function qux() { return 5; } else function qux() { return 6; }
return foo() === 2 && bar() === 3 && baz() === 4 && qux() === 6;
      
}

if (!test())
    throw new Error("Test failed");

