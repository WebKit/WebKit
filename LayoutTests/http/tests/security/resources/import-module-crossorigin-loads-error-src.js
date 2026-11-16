import("http://localhost:8000/security/resources/cors-deny.py?credentials=true").then(
    function() { done("FAIL");},
    function() { done("PASS"); });
