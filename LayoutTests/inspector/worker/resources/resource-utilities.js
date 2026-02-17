function loadResourceXHR(path) {
    let xhr = new XMLHttpRequest;
    xhr.open("GET", path, true);
    xhr.send();
}

function loadResourceFetch(path) {
    fetch(path);
}

function loadResourceDOM(path) {
    let style = document.createElement("link");
    style.rel = "stylesheet";
    style.href = path + "?" + Math.random();
    document.head.appendChild(style);
}
