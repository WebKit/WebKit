document.open();
document.write("<br>Test 2<br>File in URL: " + leafName(document.URL) + "<br>");

function leafName(url) {
    var tokens = new Array();
    tokens = url.split('/');
    var len = tokens.length;
    return tokens[len-1];
}