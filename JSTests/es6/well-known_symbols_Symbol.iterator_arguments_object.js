function test() {

return (function() {
  return typeof arguments[Symbol.iterator] === 'function'
    && Object.hasOwnProperty.call(arguments, Symbol.iterator);
}());
      
}

if (!test())
    throw new Error("Test failed");

