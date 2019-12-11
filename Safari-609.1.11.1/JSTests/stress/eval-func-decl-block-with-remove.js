var assert = function (result, expected, message) {
  if (result !== expected) {
    throw new Error('Error in assert. Expected "' + expected + '" but was "' + result + '":' + message );
  }
};

function foo() {
    function boo() { 
        return typeof a; 
    }
    eval("{ assert(boo(), 'undefined'); delete a; assert(boo(), 'undefined'); function a() { return 'text-a'; } assert(boo(), 'function');} ");
}
foo(); 

for (let i = 0; i < 10000; i++) {
    foo();
}