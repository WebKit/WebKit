<?php
  header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
  header("Cache-Control: no-cache, must-revalidate");
  header("Pragma: no-cache");
  if ($_GET["csp"]) {
    $csp = $_GET["csp"];
    // If the magic quotes option is enabled, the CSP could be escaped and
    // the test would fail.
    if (get_magic_quotes_gpc()) {
      $csp = stripslashes($csp);
    }
    header("Content-Security-Policy: " . $csp);
  } else if ($_GET["type"] == "multiple-headers") {
    header("Content-Security-Policy: connect-src 'none'");
    header("Content-Security-Policy: script-src 'self'", false);
  }
?>

<?php
if ($_GET["type"] == "eval") {
?>

var id = 0;
try {
  id = eval("1 + 2 + 3");
}
catch (e) {
}

postMessage(id === 0 ? "eval blocked" : "eval allowed");

<?php
} else if ($_GET["type"] == "function-function") {
?>

var fn = function() {
    postMessage('Function() function blocked');
}
try {
    fn = new Function("", "postMessage('Function() function allowed');");
}
catch(e) {
}
fn();

<?php
} else if ($_GET["type"] == "importscripts") {
?>

try {
    importScripts("http://localhost:8000/security/contentSecurityPolicy/resources/post-message.js");
    postMessage("importScripts allowed");
} catch(e) {
    postMessage("importScripts blocked: " + e);
}

<?php
} else if ($_GET["type"] == "make-xhr") {
?>

var xhr = new XMLHttpRequest;
xhr.addEventListener("load", function () {
    postMessage("xhr allowed");
});
xhr.addEventListener("error", function () {
    postMessage("xhr blocked");
});
xhr.open("GET", "http://127.0.0.1:8000/xmlhttprequest/resources/get.txt", true);
xhr.send();

<?php
} else if ($_GET["type"] == "set-timeout") {
?>

var id = 0;
try {
    id = setTimeout("postMessage('handler invoked')", 100);
} catch(e) {
}
postMessage(id === 0 ? "setTimeout blocked" : "setTimeout allowed");

<?php
} else if ($_GET["type"] == "alert-pass") {
?>

alert('PASS');

<?php
} else if ($_GET["type"] == "report-referrer") {
?>

var xhr = new XMLHttpRequest;
xhr.open("GET", "http://127.0.0.1:8000/security/resources/echo-referrer-header.php", true);
xhr.onload = function () {
    postMessage(this.responseText);
};
xhr.send();

<?php
} else if ($_GET["type"] == "shared-report-referrer") {
?>

onconnect = function (e) {
    var port = e.ports[0];
    var xhr = new XMLHttpRequest;
    xhr.open(
        "GET",
        "http://127.0.0.1:8000/security/resources/echo-referrer-header.php",
        true);
    xhr.onload = function () {
        port.postMessage(this.responseText);
    };
    xhr.send();
};

<?php
} else if ($_GET["type"] == "multiple-headers") {
?>

var xhr = new XMLHttpRequest;
xhr.addEventListener("load", function () {
    postMessage("xhr allowed");
});
xhr.addEventListener("error", function () {
    postMessage("xhr blocked");
});
xhr.open("GET", "http://127.0.0.1:8000/xmlhttprequest/resources/get.txt", true);
xhr.send();

var id = 0;
try {
  id = eval("1 + 2 + 3");
}
catch (e) {
}

postMessage(id === 0 ? "eval blocked" : "eval allowed");

<?php
}
?>
