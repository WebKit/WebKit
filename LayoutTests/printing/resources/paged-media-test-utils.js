function pageNumberShouldBe(id, expectedPageNumber)
{
    var actualPageNumber = layoutTestController.pageNumberForElementById(id);
    var result = '';
    if (actualPageNumber == expectedPageNumber)
        result = 'PASS';
    else
        result = 'FAIL expect page number is ' + expectedPageNumber + '. Was ' + actualPageNumber;
    document.getElementById('results').innerHTML += result + '<br>';
}
