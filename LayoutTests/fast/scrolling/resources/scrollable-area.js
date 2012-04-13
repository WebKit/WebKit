var referenceResult = 0;
var referenceResult2 = 0;
var referenceIntermediateFunction = 0;
var isDRT = window.layoutTestController && window.internals && window.internals.numberOfScrollableAreas;

function runTest(firstResult, intermediateFunction, secondResult)
{
    if (isDRT) {
        layoutTestController.waitUntilDone();
        layoutTestController.dumpAsText();
    }

    referenceResult = firstResult;
    if (intermediateFunction) {
        referenceIntermediateFunction = intermediateFunction;
        referenceResult2 = secondResult;
    }
    setTimeout(end, 0);
}

function end()
{
    var result = 0;

    if (isDRT) {
        result = internals.numberOfScrollableAreas(document);
        shouldBeTrue(stringify(result == referenceResult));
        if (referenceIntermediateFunction) {
            referenceIntermediateFunction();
            result = internals.numberOfScrollableAreas(document);
            shouldBeTrue(stringify(result == referenceResult2));
        }

        layoutTestController.notifyDone();
        return;
    }

    if (referenceIntermediateFunction)
        referenceIntermediateFunction();
}
