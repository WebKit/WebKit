var initialize_ResourceTest = function() {

InspectorTest.HARNondeterministicProperties = {
    bodySize: 1,
    compression: 1,
    headers: 1,
    headersSize: 1,
    id: 1,
    onContentLoad: 1,
    onLoad: 1,
    receive: 1,
    startedDateTime: 1,
    time: 1,
    timings: 1,
    version: 1,
    wait: 1,
};

// addObject checks own properties only, so make a deep copy rather than use prototype.

InspectorTest.HARNondeterministicPropertiesWithSize = JSON.parse(JSON.stringify(InspectorTest.HARNondeterministicProperties));
InspectorTest.HARNondeterministicPropertiesWithSize.size = 1;


InspectorTest.resourceURLComparer = function(r1, r2)
{
    return r1.request.url.localeCompare(r2.request.url);
}

}
