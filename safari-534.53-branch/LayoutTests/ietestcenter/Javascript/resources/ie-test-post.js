try {

// If we successfully parsed the test case, it should have called registerTest; this will have set 'test'.
successfullyParsed = (ES5Harness.test != undefined);

if (successfullyParsed) {
    // Construct a description string that included the test's id.
    description(ES5Harness.test.id + " (" + ES5Harness.test.path + ")\n    " + ES5Harness.test.description);

    // If no precondition is set, it is considered to pass, if one is set then check it.
    ES5Harness.preconditionPassed =
        !ES5Harness.test.precondition || ES5Harness.test.precondition();

    // If the precondition passed then call the test (so long as it exists!).
    ES5Harness.testPassed =
        ES5Harness.preconditionPassed && ES5Harness.test.test && ES5Harness.test.test();
}

} catch (e) {}

// Check the results!
shouldBeTrue("ES5Harness.preconditionPassed");
shouldBeTrue("ES5Harness.testPassed");
