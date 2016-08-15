function test() {

var passed = true;
var s = Symbol.toStringTag;
[
  [String, "String Iterator"],
  [Array, "Array Iterator"],
  [Map, "Map Iterator"],
  [Set, "Set Iterator"]
].forEach(function(pair){
  var iterProto = Object.getPrototypeOf(new pair[0]()[Symbol.iterator]());
  passed = passed
    && iterProto.hasOwnProperty(s)
    && iterProto[s] === pair[1];
});

passed = passed
  && Object.getPrototypeOf(function*(){})[s] === "GeneratorFunction"
  && Object.getPrototypeOf(function*(){}())[s] === "Generator"
  && Map.prototype[s] === "Map"
  && Set.prototype[s] === "Set"
  && ArrayBuffer.prototype[s] === "ArrayBuffer"
  && DataView.prototype[s] === "DataView"
  && Promise.prototype[s] === "Promise"
  && Symbol.prototype[s] === "Symbol"
  && typeof Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Int8Array).prototype, Symbol.toStringTag).get === "function";
  return passed;
}

if (!test())
    throw new Error("Test failed");
