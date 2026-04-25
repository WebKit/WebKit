function test() {

return (function(parts) {
  return Object.isFrozen(parts) && Object.isFrozen(parts.raw);
}) `foo${0}bar${0}baz`;
      
}

if (!test())
    throw new Error("Test failed");

