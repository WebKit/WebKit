postMessage("START");
xhr = new XMLHttpRequest;
xhr.open("GET", "endless-response.py", false);
try {
    xhr.send();
} catch (e) {
}
