// Magnitude gives a best guess estimate of the runtime of a function.
// Magnitude.run can be called multiple times in a single test if desired.
//
// Usage:
// <script src="../resources/magnitude-perf.js"></script>
// <script>
// ...
// Magnitude.run(setup, test, expected)
// </script>
//
// -setup is the function to run once before the test-loop. It takes a magnitude argument
// that is the value of "n".
// -test is the code whose runtime is being tests. It also takes a magnitude argument.
// -expected is one of the run-time constants listed below (e.g. Magnitude.CONSTANT).

if (window.testRunner)
    testRunner.dumpAsText();

// Namespace.
var Magnitude = {};

// The description of what this test is testing. Gets prepended to the dumped markup.
Magnitude.description = function(description)
{
    Magnitude._log(description);
}

Magnitude._numPoints = 10;
Magnitude._minIterations = 5;
Magnitude._maxIterations = 1000;

Magnitude.CONSTANT = "O(1)";
Magnitude.LINEAR = "O(n)";
Magnitude.LOGARITHMIC = "O(log n)";
Magnitude.POLYNOMIAL = ">=O(n^2)";
Magnitude.INDETERMINATE = "indeterminate result";

Magnitude._log = function(msg)
{
    if (!Magnitude._container)
        Magnitude._container = document.createElement('pre');
    Magnitude._container.appendChild(document.createTextNode(msg + '\n'));
}

Magnitude._debug = function(msg)
{
    Magnitude._debugLog += msg + '\n';
}

Magnitude.run = function(setup, test, expected)
{
    Magnitude._debugLog = "\nDEBUG LOG:\n";

    Magnitude._magnitudes = [];
    for (var i = 0; i < Magnitude._numPoints; i++)
        Magnitude._magnitudes.push(Math.pow(2, i));

    var milliseconds = 50;
    var runsPerIteration = 1;
    var numTries = 3;
    Magnitude._run(setup, test, expected, milliseconds, runsPerIteration, numTries);
}

Magnitude._run = function(setup, test, expected, milliseconds, runsPerIteration, numTriesLeft)
{
    Magnitude._iterations = {};
    var maxMagnitude = Magnitude._magnitudes[Magnitude._magnitudes.length - 1];

    // We want the largest magnitude to do between Magnitude._minIterations and Magnitude._maxIterations iterations.
    // If it's too fast, we increase the runsPerIteration to do more runs per iteration.
    // If it's too slow, we increase milliseconds to give each iteration more time.
    while (true) {
        var iterations = Magnitude._runIteration(setup, test, maxMagnitude, milliseconds, runsPerIteration);
        Magnitude._debug("iterations " + iterations);

        // If we get too few or too many on the largest magnitude iterations, then we can't trust this run.
        // Too many runs means the the test loop itself may be taking more time than running the test.
        if (iterations <= Magnitude._minIterations)
            milliseconds = Math.max(2, Math.floor(Magnitude._minIterations / iterations)) * milliseconds;
        else if (iterations > Magnitude._maxIterations)
            runsPerIteration = Math.max(2, Math.floor(iterations / Magnitude._maxIterations)) * runsPerIteration;
        else {
            Magnitude._iterations[maxMagnitude] = iterations;
            break;
        }
    }

    for (var i = 0; i < Magnitude._magnitudes.length - 1; i++) {
        var magnitude = Magnitude._magnitudes[i];
        Magnitude._iterations[magnitude] = Magnitude._runIteration(setup, test, magnitude, milliseconds, runsPerIteration);
    }

    Magnitude._logIterationInfo(milliseconds, runsPerIteration);

    numTriesLeft--;
    var bigO = Magnitude._bigOGuess(milliseconds);
    if (bigO == expected || numTriesLeft < 1) {
        Magnitude._log(bigO == expected ? "PASS" : "FAIL: got " + bigO + " expected " + expected);

        // By default don't log detailed information to layout test results to keep expected results
        // consistent from run to run.
        if (!window.testRunner || bigO != expected)
            Magnitude._log(Magnitude._debugLog);
    } else {
        Magnitude._debug("numTriesLeft: " + numTriesLeft);
        arguments.callee(setup, test, expected, milliseconds, runsPerIteration, numTriesLeft);
    }
}

Magnitude._rSquared = function(milliseconds, opt_xTransform, opt_yTransform)
{
    // Implement http://www.easycalculation.com/statistics/learn-correlation.php.
    // x = magnitude
    // y = iterations
    var sumX = 0;
    var sumY = 0;
    var sumXSquared = 0;
    var sumYSquared = 0;
    var sumXTimesY = 0;

    var numPoints = Magnitude._magnitudes.length;

    for (var i = 0; i < numPoints; i++) {
        var x = Magnitude._magnitudes[i];
        if (opt_xTransform)
            x = opt_xTransform(x);

        var y = milliseconds / Magnitude._iterations[Magnitude._magnitudes[i]];
        if (opt_yTransform)
            y = opt_yTransform(y);

        sumX += x;
        sumY += y;
        sumXSquared += x * x;
        sumYSquared += y * y;
        sumXTimesY += x * y;
    }

    var r = (numPoints * sumXTimesY - sumX * sumY) /
        Math.sqrt((numPoints * sumXSquared - sumX * sumX) *
                  (numPoints * sumYSquared - sumY * sumY));

    if (isNaN(r) || r == Math.Infinity)
        r = 0;

    rSquared = r * r;

    var slope = (numPoints * sumXTimesY - sumX * sumY) /
        (numPoints * sumXSquared - sumX * sumX);
    var intercept = sumY / numPoints - slope * sumX / numPoints;
    Magnitude._debug("numPoints " + numPoints + " slope " + slope + " intercept " + intercept + " rSquared " + rSquared);

    return rSquared;
}

Magnitude._logIterationInfo = function(milliseconds, runsPerIteration)
{
    var iterationsArray = [];
    for (var i = 0; i < Magnitude._magnitudes.length; i++) {
        var magnitude = Magnitude._magnitudes[i];
        var iterations = Magnitude._iterations[magnitude];
        iterationsArray.push(iterations);
        Magnitude._debug("magnitude: " + magnitude + " iterations: " + iterations + " runsPerIteration " + runsPerIteration +
            " loop-time " + milliseconds + " time/iteration(ms): " + milliseconds / iterations);
    }

    // Print out the magnitudes/arrays in CSV to afford easy copy-paste to a charting application.
    Magnitude._debug("magnitudes: " + Magnitude._magnitudes.join(','));
    Magnitude._debug("iterations: " + iterationsArray.join(','));
    Magnitude._debug("milliseconds/iteration: " + iterationsArray.map(function(iterations) {return milliseconds / iterations}).join(','));
}

Magnitude._bigOGuess = function(milliseconds)
{
    var rSquared = Magnitude._rSquared(milliseconds);
    var rSquaredXLog = Magnitude._rSquared(milliseconds, Math.log);
    var rSquaredXYLog = Magnitude._rSquared(milliseconds, Math.log, Math.log);
    Magnitude._debug("rSquared " + rSquared + " rSquaredXLog " + rSquaredXLog + " rSquaredXYLog " + rSquaredXYLog);

    var rSquaredMax = Math.max(rSquared, rSquaredXLog, rSquaredXYLog);

    var bigO = Magnitude.INDETERMINATE;

    // FIXME: These numbers were chosen arbitrarily.
    // Are there a better value to check against? Do we need to check rSquaredMax?
    if (rSquared < 0.8 && rSquaredMax < 0.9)
        bigO = Magnitude.CONSTANT;
    else if (rSquaredMax > 0.9) {
        if (rSquared == rSquaredMax)
            bigO = Magnitude.LINEAR;
        else if (rSquaredXLog == rSquaredMax)
            bigO = Magnitude.LOGARITHMIC;
        else if (rSquaredXYLog == rSquaredMax)
            bigO = Magnitude.POLYNOMIAL;
    }

    return bigO;
}

Magnitude._runIteration = function(setup, test, magnitude, milliseconds, runsPerIteration)
{
    setup(magnitude);

    var debugStr = 'run iteration. magnitude ' + magnitude + " milliseconds " + milliseconds + " runsPerIteration " + runsPerIteration;
    if (window.GCController) {
        if (GCController.getJSObjectCount)
            debugStr += " jsObjectCountBefore " + GCController.getJSObjectCount();

        // Do a gc to reduce likelihood of gc during the test run.
        // Do multiple gc's for V8 to clear DOM wrappers.
        GCController.collect();
        GCController.collect();
        GCController.collect();

        if (GCController.getJSObjectCount)
            debugStr += " jsObjectCountAfter " + GCController.getJSObjectCount();
    }

    Magnitude._debug(debugStr);

    var iterations = 0;
    if (window.chromium) {
        // FIXME: If using microseconds turns out to be less flaky, expose microseconds
        // from JSC or testRunner and use them. Otherwise, get rid of this block.
        var microseconds = milliseconds * 1000;
        var interval = new chromium.Interval();
        interval.start();
        while (interval.microseconds() < microseconds) {
            // Loop runsPerIteration times to reduce errors due to the overhead and granularity of the Date object.
            for (var i = 0; i < runsPerIteration; i++)
                test(magnitude);
            iterations++;
        }
        interval.stop();
    } else {
        var start = Date.now();
        while (Date.now() - start < milliseconds) {
            // Loop runsPerIteration times to reduce errors due to the overhead and granularity of the Date object.
            for (var i = 0; i < runsPerIteration; i++)
                test(magnitude);
            iterations++;
        }
    }
    return iterations;
}

window.addEventListener('load', function() {
    // FIXME: Add Magnitude.waitUntilDone/notifyDone for tests that need to operate after the load event has fired.
    if (window.testRunner)
        document.body.innerHTML = '';
    document.body.appendChild(Magnitude._container);
}, false);
