if (window.testRunner && testRunner.setCanOpenWindows)
    testRunner.setCanOpenWindows();

async function loadImage(document, url)
{
    const image = document.createElement("img");
    image.src = url;
    await image.decode();
    return image;
}

function pixelOfDraw(image, x, y)
{
    const canvas = image.ownerDocument.createElement("canvas");
    canvas.width = 100;
    canvas.height = 100;
    const context = canvas.getContext("2d");
    context.drawImage(image, 0, 0, 100, 100);
    return Array.from(context.getImageData(x, y, 1, 1).data);
}

function openSecondPage(test)
{
    const popup = window.open("about:blank");
    test.add_cleanup(() => popup.close());
    return popup;
}

function alphaOfDraw(image)
{
    const canvas = image.ownerDocument.createElement("canvas");
    canvas.width = 100;
    canvas.height = 100;
    const context = canvas.getContext("2d");
    context.drawImage(image, 0, 0, 100, 100);
    const data = context.getImageData(0, 0, 100, 100).data;
    let alpha = "";
    for (let i = 3; i < data.length; i += 4)
        alpha += data[i] > 127 ? "#" : ".";
    return alpha;
}
