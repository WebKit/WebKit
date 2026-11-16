(()=>{
function inner(x) {
    return "inner" + x;
}
  function middle(x) {
    let y = "middle";
    y += inner(x);
    return y;
}
   globalThis.outer = function(x) {
       let y = "outer";
       y += middle(x);
       return y;
   };
})();
//# sourceMappingURL=range-mappings.js.map
