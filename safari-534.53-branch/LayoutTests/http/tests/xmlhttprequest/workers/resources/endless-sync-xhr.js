postMessage("START");
xhr = new XMLHttpRequest;
xhr.open("GET", "endless-response.php", false);
try {
    xhr.send();
} catch (e) {
}
