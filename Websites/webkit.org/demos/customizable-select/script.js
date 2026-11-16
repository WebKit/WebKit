/* Filtering */

const categorySelect = document.querySelector("#category-select");
const colorSelect = document.querySelector("#color-select");

function matchesAllFilters(figure) {
    if (!figure.querySelector("img").src.includes(categorySelect.value) && categorySelect.value != "everything")
        return false;
    if (figure.querySelector("img").dataset.color != colorSelect.value && colorSelect.value != "all")
        return false;
    return true;
}

function applyAllFilters() {
    const figureList = document.querySelectorAll("main > figure");
    figureList.forEach((figure) => {
        figure.hidden = !matchesAllFilters(figure);
    });
}

categorySelect.addEventListener("change", () => {
    applyAllFilters();
});

colorSelect.addEventListener("change", () => {
    applyAllFilters();
});

/* Sorting */

const sortSelect = document.querySelector("#sort-select");
sortSelect.addEventListener("change", () => {
    const wasReversed = document.querySelector(".grid").classList.contains("reverse");
    const isReversed = sortSelect.value == "oldest";
    document.querySelector(".grid").classList.toggle("reverse", isReversed);

    if (wasReversed != isReversed) {
        if (document.startViewTransition) {
            document.startViewTransition(() => {
                reverse();
            });
        } else {
            reverse();
        }
    }
        
});

function reverse() {
    const container = document.querySelector("main");
    const reversedList = [...document.querySelectorAll("main > figure")].reverse();
    reversedList.forEach(child => container.append(child));
}

/* Image overlay appearing on click */

const dialog = document.querySelector("dialog");
document.querySelector("main").addEventListener("click", (e) => {
    if (e.target.localName != "img")
        return;
    dialog.querySelector("img").src = e.target.src;
    dialog.showModal();
});

dialog.addEventListener("click", (e) => {
    if (e.target.localName != "img")
        dialog.close();
});
