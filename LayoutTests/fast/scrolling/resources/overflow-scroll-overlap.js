
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    internals.settings.setAsyncFrameScrollingEnabled(true);
    internals.settings.setAsyncOverflowScrollingEnabled(true);
}

function sleep(delay)
{
    return new Promise((resolve) => { setTimeout(resolve, delay); });
}

async function runTest() {
    for (const scrollable of document.querySelectorAll('.overflowscroll')) {
        scrollable.addEventListener('scroll', function(e) {
            logScroll(e.target);
        });
    }

    {
        let i = 0;
        for (const scrollcontent of document.querySelectorAll('.scrollcontent'))
            scrollcontent.innerText = "Scrollable " + ++i;
    }
    {
        let i = 0;
        for (const overlapping of document.querySelectorAll('.overlapping'))
            overlapping.insertBefore(document.createTextNode("Overlapping " + ++i), overlapping.firstChild);
    }


    if (!window.testRunner || !testRunner.runUIScript)
        return;

    for (const testcase of document.querySelectorAll('.case'))
        testcase.style.display = 'none';

    {
        let i = 0;
        for (const testcase of document.querySelectorAll('.case')) {
            ++i;
            testcase.style.display = 'inline-block';

            const target = testcase.querySelector('.target');
            const rect = target.getBoundingClientRect();
            const centerX = (rect.left + rect.right) / 2;
            const centerY = (rect.top + rect.bottom) / 2;
            await touchAndDragFromPointToPoint(centerX, centerY, centerX, centerY - 30);
            await liftUpAtPoint(centerX, centerY - 30);
            await sleep(500);

            testcase.style.display = 'none';
            outputCase(i);
        }
   }

    for (const testcase of document.querySelectorAll('.case'))
        testcase.style.display = 'none';

    testRunner.notifyDone();
}

const scrolledElements = new Set();

function logScroll(element) {
    if (scrolledElements.has(element))
        return;
    scrolledElements.add(element);
}

function outputCase(i) {
    log.innerText += "case " + i + ": ";
    for (const scrolled of scrolledElements)
        log.innerText += scrolled.getElementsByClassName("scrollcontent")[0].innerText + " ";
    log.innerText += "\n";
    scrolledElements.clear();
}
