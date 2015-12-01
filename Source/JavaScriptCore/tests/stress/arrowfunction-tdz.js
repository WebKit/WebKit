var A = class A { };
var B = class B extends A {
  constructor(accessThisBeforeSuper) {
    if (accessThisBeforeSuper) {
      var f = () => this;
      super();
    } else {
      super();
    }
  }
};

var exception = null;
for (var i=0; i<10000; i++) {
  try {
       new B(true);
  } catch (e) {
      exception = e;
      if (!(e instanceof ReferenceError))
          throw "Exception thrown was not a reference error";
  }

  if (!exception)
      throw "Exception not thrown for an unitialized this at iteration";

  var e = new B(false);
}
