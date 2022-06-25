var jsTestIsAsync = true;

function finishTest(error)
{
    if (error) {
        if (error.toString)
            error = error.toString();

        if (typeof error === 'string')
            testFailed(error);
        else
            debug("WARN: cannot print error because it is not a string");
    }

    finishJSTest();
}