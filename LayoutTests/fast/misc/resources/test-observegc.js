description("Ensures that window.internals.observegc works as expected");

var testObject = { testProperty : "testValue" };

var observer = internals.observeGC(testObject);
testObject = null;
gc();

shouldBe('observer.wasCollected', 'true');
