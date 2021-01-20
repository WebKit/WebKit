import("http://localhost:8000/security/resources/cors-deny.php?credentials=true").then(
    function() { done("FAIL");},
    function() { done("PASS"); });
