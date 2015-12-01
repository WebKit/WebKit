description('Tests for ES6 arrow function test tdz');

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

var isReferenceError = false;
try {
     new B(true);
} catch (e) {
    isReferenceError = e instanceof ReferenceError;
}

shouldBe('isReferenceError', 'true');

var e = new B(false);

var successfullyParsed = true;
