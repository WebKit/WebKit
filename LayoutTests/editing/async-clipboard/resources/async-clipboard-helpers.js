function sampleBase64PNGImageData() {
    // A 60 by 60 solid red square.
    return "iVBORw0KGgoAAAANSUhEUgAAADwAAAA8CAYAAAA6/NlyAAAAAXNSR0IArs4c6QAAAJZlWElmTU0AKgAAAAgABAEaAAUAAAABAAAAPgEbAAUAAAABAAAARgEoAAMAAAABAAIAAIdpAAQAAAABAAAATgAAAAAAAACQAAAAAQAAAJAAAAABAASShgAHAAAAEgAAAISgAQADAAAAAQABAACgAgAEAAAAAQAAADygAwAEAAAAAQAAADwAAAAAQVNDSUkAAABTY3JlZW5zaG90CUg0mwAAAAlwSFlzAAAWJQAAFiUBSVIk8AAAAdRpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IlhNUCBDb3JlIDUuNC4wIj4KICAgPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4KICAgICAgPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIKICAgICAgICAgICAgeG1sbnM6ZXhpZj0iaHR0cDovL25zLmFkb2JlLmNvbS9leGlmLzEuMC8iPgogICAgICAgICA8ZXhpZjpQaXhlbFhEaW1lbnNpb24+NjA8L2V4aWY6UGl4ZWxYRGltZW5zaW9uPgogICAgICAgICA8ZXhpZjpVc2VyQ29tbWVudD5TY3JlZW5zaG90PC9leGlmOlVzZXJDb21tZW50PgogICAgICAgICA8ZXhpZjpQaXhlbFlEaW1lbnNpb24+NjA8L2V4aWY6UGl4ZWxZRGltZW5zaW9uPgogICAgICA8L3JkZjpEZXNjcmlwdGlvbj4KICAgPC9yZGY6UkRGPgo8L3g6eG1wbWV0YT4K9+BmbAAAABxpRE9UAAAAAgAAAAAAAAAeAAAAKAAAAB4AAAAeAAAAls/GUI8AAABiSURBVGgF7NIxAQAgDMRA6ogd/7pAQByEdPzt/zr37Ls+uqmwXDthOfBKOGHZAr20DBR1EsYksiBhGSjqJIxJZEHCMlDUSRiTyIKEZaCokzAmkQUJy0BRJ2FMIgsSloGizgMAAP//Y4b2JQAAAGBJREFU7dIxAQAgDMRA6ogd/7pAQByEdPzt/zr37Ls+uqmwXDthOfBKOGHZAr20DBR1EsYksiBhGSjqJIxJZEHCMlDUSRiTyIKEZaCokzAmkQUJy0BRJ2FMIgsSloGizgOl3pDZBBa0hwAAAABJRU5ErkJggg";
}

function imageBlob(base64String = sampleBase64PNGImageData()) {
    return textBlob(atob(base64String), "image/png");
}

function textBlob(data, type = "text/plain") {
    const array = [...data].map(char => char.charCodeAt(0));
    return new Blob([new Uint8Array(array)], { "type" : type });
}

function loadText(blob) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader;
        reader.addEventListener("load", () => resolve(reader.result), { once: true });
        reader.addEventListener("error", reject, { once: true });
        reader.readAsText(blob);
    });
}

function loadImage(blob) {
    return new Promise((resolve, reject) => {
        const image = new Image;
        image.addEventListener("load", () => resolve(image), { once: true });
        image.addEventListener("error", reject, { once: true });
        image.src = URL.createObjectURL(blob);
    });
}
