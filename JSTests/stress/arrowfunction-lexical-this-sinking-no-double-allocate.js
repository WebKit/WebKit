var newContext = {
  id : 'new-context'
};

function call(o) { o.x = 3; }
noInline(call);

// Should be invoced by call with substitute this by newContext
function sink (p, q) {
    var f = () => {
      // Check if arrow function store context
      if (this != newContext || this.id != newContext.id)
          throw 'Wrong context of arrow function #1';
    };
    if (p) {
        call(f); // Force allocation of f
        if (q) {
            OSRExit();
        }
        return f;
    }
    return { 'x': 2 };
}
noInline(sink);

for (var i = 0; i < 100000; ++i) {
    var o = sink.call(newContext, true, false);
    if (o.x != 3)
        throw "Error: expected o.x to be 2 but is " + result;
}

// At this point, the arrow function should be compiled down to the FTL

// Check that the function is properly allocated on OSR exit
var f = sink(true, true);
if (f.x != 3)
    throw "Error: expected o.x to be 3 but is " + result;
