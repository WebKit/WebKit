function clearLocationCookie(url) {
    document.cookie = 'location=;path=/;expires=' + (new Date()).toGMTString() + ';';
}

function setLocationCookie(url) {
    clearLocationCookie(url);
    document.cookie = 'location=' + url + ';path=/';
}

function log(s) {
    var console = document.getElementById("console");
    if (!console) {
        console = document.createElement("pre");
        document.body.appendChild(console);
    }
    console.innerText += s + "\n";
}

onpopstate = function() {
    log("popstate: " + event.state);
}
