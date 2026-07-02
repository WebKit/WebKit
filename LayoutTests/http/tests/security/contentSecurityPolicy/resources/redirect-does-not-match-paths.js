test(function () {
    assert_true(typeof(script_loaded) !== "undefined");
}, 'CSP ignores paths of redirected resources in matching algorithm for scripts.');

async_test(function(t) {
    var img = document.createElement("img");
    img.onload = function() {
        t.step(function () { assert_true(true); t.done(); });
    };
    img.src = "http://localhost:8000/security/contentSecurityPolicy/resources/redirect.pl?type=image";
    document.body.appendChild(img);
}, 'CSP ignores paths of redirect resources in matching algorithm for images.');

async_test(function(t) {
    window.addEventListener("message", function () {
        t.step(function () { assert_true(true); t.done(); });
    });
    var iframe = document.createElement("iframe");
    iframe.src = "http://localhost:8000/security/contentSecurityPolicy/resources/redirect.pl?type=frame";
    document.body.appendChild(iframe);
}, 'CSP ignores paths of redirect resources in matching algorithm for frames.');

test(function () {
    assert_true(getComputedStyle(document.body).color === "rgb(0, 0, 255)");
}, 'CSP ignores paths of redirected resources in matching algorithm for stylesheets.');

async_test(function (t) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
        t.step(function () { assert_true(xhr.status === 200); t.done(); });
    };
    xhr.open("GET", "http://localhost:8000/security/contentSecurityPolicy/resources/redirect.pl?type=xhr", true);
    xhr.send();
}, 'CSP ignores paths of redirect resources in matching algorithm for XHR.');
