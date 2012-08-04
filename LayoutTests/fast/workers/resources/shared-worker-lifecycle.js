description("This test checks whether shared workers exit when the parent document closes");

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    waitUntilWorkerThreadsExit(runTests);
} else {
    debug("NOTE: This test relies on functionality in DumpRenderTree to detect when workers have exited - test results will be incorrect when run in a browser.");
    runTests();
}

function runTests()
{
    createWorkerFrame("frame1", "worker1");
    createWorkerFrame("frame2", "worker1,worker2");
    createWorkerFrame("frame3", "worker3");
    createWorkerFrame("frame4", "worker1");

    waitUntilThreadCountMatches(closeFrame1, 3);
}

function createWorkerFrame(id, workerNames)
{
    var iframe = document.createElement("iframe");
    iframe.setAttribute("id", id);
    iframe.setAttribute("src", "resources/create-shared-worker-frame.html?" + workerNames);
    document.body.appendChild(iframe);
}

function closeFrame(id)
{
    var frame = document.getElementById(id);
    frame.parentNode.removeChild(frame);
}

function closeFrame1()
{
    closeFrame("frame1");
    ensureThreadCountMatches(closeFrame2, 3);
}

function closeFrame2()
{
    testPassed("Frame1 closed, shared workers kept running");
    closeFrame("frame2");
    ensureThreadCountMatches(closeFrame3, 2);
}

function closeFrame3()
{
    testPassed("Frame2 closed, shared worker2 exited");
    closeFrame("frame3");
    ensureThreadCountMatches(closeFrame4, 1);
}

function closeFrame4()
{
    testPassed("Frame3 closed, shared worker3 exited");
    closeFrame("frame4");
    waitUntilWorkerThreadsExit(complete);
}

function complete()
{
    testPassed("Frame4 closed, all workers closed");
    done();
}
