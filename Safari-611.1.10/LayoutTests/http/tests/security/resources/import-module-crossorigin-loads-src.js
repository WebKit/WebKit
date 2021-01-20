// Executed with "omit".
// https://github.com/tc39/proposal-dynamic-import/blob/master/HTML%20Integration.md
import("http://localhost:8000/security/resources/cors-script.php?credentials=false").then(
    function() { done("PASS");},
    function() { done("FAIL"); });

