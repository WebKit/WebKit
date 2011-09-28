var initialize_SearchTest = function() {

InspectorTest.dumpSearchResults = function(searchResults)
{
    InspectorTest.addResult("Search results: ");
    for (var i = 0; i < searchResults.length; i++)
        InspectorTest.addResult("url: " + searchResults[i].url + ", matchesCount: " + searchResults[i].matchesCount);
    InspectorTest.addResult("");
}

};
