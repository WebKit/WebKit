var initialize_SearchTest = function() {

InspectorTest.dumpSearchResults = function(searchResults)
{
    function comparator(a, b)
    {
        a.url.localeCompare(b.url);
    }
    searchResults.sort(comparator);

    InspectorTest.addResult("Search results: ");
    for (var i = 0; i < searchResults.length; i++)
        InspectorTest.addResult("url: " + searchResults[i].url + ", matchesCount: " + searchResults[i].matchesCount);
    InspectorTest.addResult("");
};

InspectorTest.dumpSearchMatches = function(searchMatches)
{
    InspectorTest.addResult("Search matches: ");
    for (var i = 0; i < searchMatches.length; i++)
        InspectorTest.addResult("lineNumber: " + searchMatches[i].lineNumber + ", line: '" + searchMatches[i].lineContent + "'");
    InspectorTest.addResult("");
};

};
