const manyGradientsColors = ["rgb(255, 0, 0)", "rgb(0, 128, 0)", "rgb(0, 0, 255)", "rgb(255, 165, 0)",
    "rgb(128, 0, 128)", "rgb(0, 128, 128)", "rgb(128, 128, 0)", "rgb(255, 0, 255)",
    "rgb(0, 0, 0)", "rgb(128, 0, 0)", "rgb(0, 0, 128)", "rgb(0, 255, 0)"];

// stopColor(color, element) returns the CSS color to use in the gradient for `color`.
function setGradient(element, direction, color, stopColor)
{
    const stop = stopColor(color, element);
    element.style.backgroundImage = `repeating-linear-gradient(${direction}, ${stop} 0 3px, transparent 3px 6px)`;
}

function createRows(stopColor)
{
    const elements = [];
    for (const [className, direction] of [["", "to bottom"], ["", "to bottom"], ["", "to right"], ["offset", "to bottom"], ["", "to bottom"]]) {
        const row = document.createElement("div");
        row.className = "row " + className;
        for (const color of manyGradientsColors) {
            const element = document.createElement("div");
            element.dataset.direction = direction;
            setGradient(element, direction, color, stopColor);
            row.appendChild(element);
            elements.push(element);
        }
        document.body.appendChild(row);
    }
    return elements;
}

// Gives every third element of the last row a different color, so its gradient changes after it was drawn.
function changeSomeGradients(elements, stopColor)
{
    const lastRow = elements.slice(-manyGradientsColors.length);
    for (let i = 0; i < lastRow.length; i += 3)
        setGradient(lastRow[i], lastRow[i].dataset.direction, manyGradientsColors[(i + 5) % manyGradientsColors.length], stopColor);
}
