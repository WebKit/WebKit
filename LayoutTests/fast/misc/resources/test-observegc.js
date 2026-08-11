description("Ensures that window.internals.observegc works as expected");

var observers = [];
for (let i = 0; i < 1000; ++i) {
    let testObject = { testProperty : "testValue" };
    let observer = internals.observeGC(testObject);
    observers.push(observer);
    testObject = null;
}

gc();

var anyCollected = false;
for (let observer of observers) {
    if (observer.wasCollected)
        anyCollected = true;
}

shouldBe('anyCollected', 'true');
