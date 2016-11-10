function loadResourceXHR() {
    let xhr = new XMLHttpRequest;
    xhr.open("GET", "dataXHR.json", true);
    xhr.send();
}

function loadResourceFetch() {
    fetch("dataFetch.json");
}
