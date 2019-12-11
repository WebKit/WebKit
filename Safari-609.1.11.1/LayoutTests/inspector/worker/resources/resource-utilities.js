function loadResourceXHR(path) {
    let xhr = new XMLHttpRequest;
    xhr.open("GET", path, true);
    xhr.send();
}

function loadResourceFetch(path) {
    fetch(path);
}
