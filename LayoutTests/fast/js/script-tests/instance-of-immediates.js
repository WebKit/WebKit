description('This test makes sure that instance of behaves correctly when the value, constructor, or its prototype are immediates.');

// A Constructor to use check for instances of, and an instance called obj.
function Constructor() {}
var obj = new Constructor();

// Run a batch of tests; call'testInstanceOf' three times, passing 1, {}, and the object 'obj', which is an instance of Constructor.
function testSet(constructor, testMethod)
{
    testMethod["1"]("(1 instanceof " + constructor + ")");
    testMethod["{}"]("({} instanceof " + constructor + ")");
    testMethod["obj"]("(obj instanceof " + constructor + ")");
}

// Test set 1, test passsing the integer 1 as the constructor to be tested for.
// The constructor being an object is the first thing tested, so these should all throw.
testSet("1", { "1":shouldThrow, "{}":shouldThrow, "obj":shouldThrow });

// Test set 2, test passsing an empty object ({}) as the constructor to be tested for.
// As well as being an object, the constructor must implement 'HasInstance' (i.e. be a function), so these should all throw too.
testSet("{}", { "1":shouldThrow, "{}":shouldThrow, "obj":shouldThrow });

// Test set 3, test passsing Constructor as the constructor to be tested for.
// Nothing should except, the third test should pass, since obj is an instance of Constructor.
testSet("Constructor", { "1":shouldBeFalse, "{}":shouldBeFalse, "obj":shouldBeTrue });

// Test set 4, test passsing Constructor as the constructor to be tested for - with Constructor.prototype set to the integer 1.
// Constructor.prototype being a non-object will cause an exception, /unless/ value is also a non-object, since this is checked first.
Constructor.prototype = 1;
testSet("Constructor", { "1":shouldBeFalse, "{}":shouldThrow, "obj":shouldThrow });

// Test set 5, test passsing Constructor as the constructor to be tested for - with Constructor.prototype set to an empty object ({}).
// All test fail, no reason to throw.  (obj instanceof Constructor) is now false, since Constructor.prototype has changed.
Constructor.prototype = {};
testSet("Constructor", { "1":shouldBeFalse, "{}":shouldBeFalse, "obj":shouldBeFalse });

// Test set 6, test passsing Constructor as the constructor to be tested for - with Constructor.prototype set to null.
// Test that behaviour is the same as for test set 4.
Constructor.prototype = null;
testSet("Constructor", { "1":shouldBeFalse, "{}":shouldThrow, "obj":shouldThrow });

var successfullyParsed = true;
