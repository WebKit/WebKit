var newContext = {
  id : 'new-context'
};

// Should be invoced by call with substitute this by newContext
function sink (p, q) {
    var g = x => {
      // Check if arrow function store context
      if (this != newContext || this.id != newContext.id)
          throw 'Wrong context of arrow function #1';
      return x;
    };
    if (p) { if (q) OSRExit(); return g; }
    return x => {
      // Check if arrow function store context
      if (this != newContext || this.id != newContext.id)
          throw 'Wrong context of arrow function #2';
      return x;
    };
}
noInline(sink);

for (var i = 0; i < 10000; ++i) {
    var f = sink.call(newContext, true, false);// Substitute this
    var result = f(42);
    if (result != 42)
    throw "Error: expected 42 but got " + result;
}

// At this point, the function should be compiled down to the FTL

// Check that the function is properly allocated on OSR exit
var f = sink.call(newContext,true, true);// Substitute this
var result = f(42);
if (result != 42)
    throw "Error: expected 42 but got " + result;
