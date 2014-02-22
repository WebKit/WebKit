function thisTest() {
    var s = document.createElement('script');
    s.innerText = "alert('FAIL.');"
    document.body.appendChild(s);
}
window.onload = runTest;
