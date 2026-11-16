//@ skip if $architecture == "x86"

var n = 10000000;

var newContext = {
  id : 'new-context'
};

function bar() { }

function foo(b) {
    var result = 0;
    var set = (x) => {
      // Check if arrow function store context
      if (this != newContext || this.id != newContext.id)
          throw 'Wrong context of arrow function';
      result = x;
    }
    if (b) {
        OSRExit();
        if (b) {
            bar(set);
        }
        return result;
    }
    return result;
}

noInline(bar);
noInline(foo);

for (var i = 0; i < n; i++) {
    var result = foo.call(newContext, !(i % 100));
    if (result != 0)
        throw "Error: bad result: " + result;
}
