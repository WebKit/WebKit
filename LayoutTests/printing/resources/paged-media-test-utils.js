// The buffer to store the results.  We output the results after all
// tests finish.   Note that we cannot have a DOM element where the
// results are stored in HTMLs because the DOM element to store
// results may change the number of pages.
var _results = '';
var _errored = false;

function appendResult(result)
{
    _results += '<br>' + result;
}

function pageNumberForElementShouldBe(id, expectedPageNumber)
{
    var actualPageNumber = layoutTestController.pageNumberForElementById(id);
    if (actualPageNumber == expectedPageNumber)
        appendResult('PASS: page number of "' + id + '" is ' + actualPageNumber);
    else {
        appendResult('FAIL: expected page number of "' + id + '" is ' + expectedPageNumber + '. Was ' + actualPageNumber);
        _errored = true;
    }
}

function numberOfPagesShouldBe(expectedNumberOfPages)
{
    var actualNumberOfPages = layoutTestController.numberOfPages();
    if (actualNumberOfPages == expectedNumberOfPages)
        appendResult('PASS: number of pages is ' + actualNumberOfPages);
    else {
        appendResult('FAIL: expected number of pages is ' + expectedNumberOfPages + '. Was ' + actualNumberOfPages);
        _errored = true;
    }
}

function runPrintingTest(testFunction)
{
    if (window.layoutTestController) {
        try {
            testFunction();
        } catch (err) {
            _results += '<p>Exception: ' + err.toString();
            _errored = true;
        }

        if (!_errored)
            _results += '<br>All tests passed';
    } else {
        _results += 'This test requires layoutTestController. You can test this manually with the above description.';
    }

    var resultElement = document.createElement('p');
    resultElement.innerHTML = _results;
    document.body.appendChild(resultElement);
}
