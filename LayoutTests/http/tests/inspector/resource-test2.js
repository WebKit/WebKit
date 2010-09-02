var initialize_ResourceTest = function() {

InspectorTest.HARNondeterministicProperties = {
    startedDateTime: 1,
    time: 1,
    wait: 1,
    receive: 1,
    headers: 1,
};

InspectorTest.resourceURLComparer = function(r1, r2)
{
    return r1.request.url.localeCompare(r2.request.url);
}

}
