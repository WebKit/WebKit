function loadResourceXHR(path) {
    let xhr = new XMLHttpRequest;
    xhr.open("GET", path, true);
    xhr.send();
}

function loadResourceFetch(path) {
    fetch(path);
}

function loadResourceDOM(path) {
    let img = document.body.appendChild(document.createElement("img"));
    img.src = path + "?" + Math.random();
}
