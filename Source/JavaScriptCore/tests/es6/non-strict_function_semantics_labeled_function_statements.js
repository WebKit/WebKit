function test() {

// Note: only available outside of strict mode.
if (!this) return false;

label: function foo() { return 2; }
return foo() === 2;
      
}

if (!test())
    throw new Error("Test failed");

