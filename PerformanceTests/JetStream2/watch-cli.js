dumpJSONResults = true;

testIterationCount = 15;

testList = [
    {name: "FlightPlanner"},
    {name: "UniPoker"},
    {name: "Air"},
    {name: "Basic"},
    {name: "ML", iterations: 7, worstCaseCount: 2},
    {name: "Babylon"},
    {name: "cdjs", iterations: 10, worstCaseCount: 2},
    {name: "first-inspector-code-load"},
    {name: "multi-inspector-code-load"},
    {name: "Box2D"},
    {name: "octane-code-load"},
    {name: "crypto"},
    {name: "delta-blue"},
    {name: "earley-boyer"},
    {name: "gbemu", iterations: 10, worstCaseCount: 2},
    {name: "navier-stokes"},
    {name: "pdfjs"},
    {name: "raytrace"},
    {name: "regexp"},
    {name: "richards"},
    {name: "splay"},
    {name: "ai-astar"},
    {name: "gaussian-blur", iterations: 10, worstCaseCount: 2},
    {name: "stanford-crypto-aes"},
    {name: "stanford-crypto-pbkdf2"},
    {name: "stanford-crypto-sha256"},
    {name: "json-stringify-inspector"},
    {name: "json-parse-inspector"},
    {name: "async-fs", iterations: 8, worstCaseCount: 2},
    {name: "hash-map", iterations: 12, worstCaseCount: 3},
    {name: "3d-cube-SP"},
    {name: "3d-raytrace-SP"},
    {name: "base64-SP"},
    {name: "crypto-aes-SP"},
    {name: "crypto-md5-SP"},
    {name: "crypto-sha1-SP"},
    {name: "date-format-tofte-SP"},
    {name: "date-format-xparb-SP"},
    {name: "n-body-SP"},
    {name: "regex-dna-SP"},
    {name: "string-unpack-code-SP"},
    {name: "tagcloud-SP"},
    {name: "chai-wtb", iterations: 5, worstCaseCount: 2},
    {name: "jshint-wtb", iterations: 5, worstCaseCount: 2},
    {name: "prepack-wtb", iterations: 5, worstCaseCount: 2}
];

testIterationCountMap = new Map;
testWorstCaseCountMap = new Map;
for (let test of testList) {
    if (test.iterations)
        testIterationCountMap.set(test.name, test.iterations);
    if (test.worstCaseCount)
        testWorstCaseCountMap.set(test.name, test.worstCaseCount);
}


testList = testList.map(x => x.name);

load("./cli.js");
