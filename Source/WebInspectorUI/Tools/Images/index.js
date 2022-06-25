const URLPrefix = "../../UserInterface/Images/";

let s_paths = new Set;
let s_size = null;


// Controls and handlers.

let s_clearButton = document.getElementById("clear");
let s_imagePicker = document.getElementById("image-picker");
let s_imageInput = document.getElementById("image-input");
let s_imageSizeSlider = document.getElementById("image-size-slider");
let s_imageSizeInput = document.getElementById("image-size-input");
let s_previewContainer = document.getElementById("preview");

s_clearButton.addEventListener("click", async (event) => {
    s_imageInput.value = "";
    s_previewContainer.textContent = "";

    s_paths.clear();

    updateURL();
});

s_imagePicker.addEventListener("input", async (event) => {
    s_imagePicker.disabled = true;
    s_clearButton.disabled = true;

    for (let path of Array.from(s_imagePicker.files).map((file) => file.name).sort())
        await loadImage(path);

    s_imagePicker.disabled = false;
    s_imagePicker.value = null;
    s_clearButton.disabled = false;

    updateURL();
});
s_imageInput.addEventListener("change", async (event) => {
    let paths = s_imageInput.value.split("\n").filter((item) => item.trim().length);

    s_imageInput.value = "";
    s_previewContainer.textContent = "";

    s_paths.clear();

    for (let path of paths)
        await loadImage(path);

    updateURL();
});

s_imageSizeSlider.addEventListener("input", async (event) => {
    changeSize(s_imageSizeSlider.value);

    updateURL();
});
s_imageSizeInput.addEventListener("input", async (event) => {
    changeSize(s_imageSizeInput.value);

    updateURL();
});


// Helper functions.

async function loadImage(path) {
    if (path.endsWith("svg"))
        return parseSVG(path);
    return loadPNG(path);
}

async function parseSVG(path) {
    let response = await fetch(normalizePath(path));
    let text = await response.text();
    let dom = (new DOMParser).parseFromString(text, "image/svg+xml");

    let imgs = [];
    if (!path.includes("#")) {
        let variants = dom.querySelectorAll(":root > :matches(svg, g)[id]");
        if (variants.length) {
            for (let variant of variants) {
                let target = variant.getAttribute("id");
                let img = await loadPNG(path + "#" + target);
                imgs.push(imgs);
            }
        }
    }
    if (!imgs.length)
        imgs.push(await loadPNG(path));

    return imgs;
}

async function loadPNG(path) {
    s_paths.add(path);

    let existingText = s_imageInput.value;
    if (existingText)
        existingText += "\n";
    s_imageInput.value = existingText + path;
    s_imageInput.scrollTop = s_imageInput.scrollHeight;

    let img = s_previewContainer.appendChild(document.createElement("img"));
    img.title = path;
    img.src = normalizePath(path);
    if (/\blight\b|Light|Black/i.test(path))
        img.classList.add("light");
    if (/\bdark\b|Dark|White/i.test(path))
        img.classList.add("dark");
    img.addEventListener("click", async (event) => {
        navigator.clipboard.writeText(path);
    });
    return img;
}

function normalizePath(path) {
    if (!path.startsWith(URLPrefix))
        path = URLPrefix + path;
    return path;
}

function changeSize(size) {
    s_size = size;

    s_previewContainer.style.setProperty("--image-size", s_size + "px");
    s_imageSizeInput.value = s_size;
    s_imageSizeSlider.value = s_size;
}

function updateURL() {
    url.search = "";

    for (let path of s_paths)
        url.searchParams.append("path", path);
    url.searchParams.append("size", s_size);
    window.history.replaceState(null, document.title, url.href);
}


// Parse URL for initial configuration.

let url = new URL(window.location.href);
(async function() {
    for (let path of url.searchParams.getAll("path"))
        await loadImage(path);
})();
changeSize(url.searchParams.get("size") ?? 16);
