var iframe = document.getElementById("blob-frame");
var html = [
    "<!DOCTYPE html>",
    "<html><body>",
    "<p id='result'>PASS: Inline script was blocked by CSP.</p>",
    "<script>",
    "document.getElementById('result').textContent = 'FAIL: Inline script executed (CSP policy was dropped).';",
    "</" + "script>",
    "</body></html>"
].join("\n");
var blob = new Blob([html], { type: "text/html" });
iframe.src = URL.createObjectURL(blob);
