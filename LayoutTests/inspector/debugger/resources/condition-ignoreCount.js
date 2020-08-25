window.CONDITION_TEST = 0;

function trigger() {
    ++window.CONDITION_TEST;

    TestPage.dispatchEventToFrontend("TestPage_trigger");
}
