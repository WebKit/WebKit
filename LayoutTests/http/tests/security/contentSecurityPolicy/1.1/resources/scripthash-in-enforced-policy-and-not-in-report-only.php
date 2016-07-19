<?
    header("Content-Security-Policy: script-src 'sha256-c8f8z1SC90Yj05k41+FT0HF/rrGJP94TPLhRvGGE8eM='");
    header("Content-Security-Policy-Report-Only: script-src 'none'");
?>
<!DOCTYPE html>
<html>
<body>
<p id="script-with-hash-result">FAIL did not execute script with hash.</p>
<p id="script-without-hash-result">PASS did not execute script without hash.</p>
<script>document.getElementById("script-with-hash-result").textContent = "PASS did execute script with hash.";</script> <!-- sha256-c8f8z1SC90Yj05k41+FT0HF/rrGJP94TPLhRvGGE8eM= -->
<script>document.getElementById("script-without-hash-result").textContent = "FAIL did execute script without hash.";</script>
</body>
</html>
