function paintIntoSwatch() {
    const canvas = document.querySelector("canvas");
    const context = canvas.getContext("2d");
    context.save();
    context.clearRect(0, 0, 40, 40);

    const button = canvas.querySelector("button");
    context.beginPath();
    context.rect(10, 10, 20, 20);
    button.focus();
    context.drawFocusIfNeeded(button);
    button.blur();

    context.restore();

    let averageRed = 0;
    let averageGreen = 0;
    let averageBlue = 0;
    let pixelCount = 0;

    const imageData = context.getImageData(0, 0, canvas.width, canvas.height).data;
    for (let row = 0; row < 40; row++) {
        for (let column = 0; column < 40; column++) {
            const byteOffset = 4 * (row * 40 + column);
            const alphaComponent = imageData.at(byteOffset + 3);
            if (!alphaComponent)
                continue;

            const redComponent = imageData.at(byteOffset);
            const greenComponent = imageData.at(byteOffset + 1);
            const blueComponent = imageData.at(byteOffset + 2);

            averageRed += redComponent;
            averageGreen += greenComponent;
            averageBlue += blueComponent;
            pixelCount++;
        }
    }

    averageRed /= pixelCount;
    averageGreen /= pixelCount;
    averageBlue /= pixelCount;

    const colorComponents = `${Math.round(averageRed)}, ${Math.round(averageGreen)}, ${Math.round(averageBlue)}`;
    const swatch = document.getElementById("swatch");
    swatch.textContent = colorComponents;
    swatch.style.backgroundColor = `rgb(${colorComponents})`;
}
