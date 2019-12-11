function test() {

var symbol = Symbol.for('foo');
return Symbol.for('foo') === symbol &&
   Symbol.keyFor(symbol) === 'foo';
      
}

if (!test())
    throw new Error("Test failed");

