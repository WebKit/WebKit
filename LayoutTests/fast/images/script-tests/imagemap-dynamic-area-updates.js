description("Test that when image map areas have their shape or coordinate dynamically altered, the clickable region changes.");

if (!window.eventSender)
    debug("This test will only work properly inside DRT.");

var image = document.createElement('img');
image.setAttribute('usemap', '#m');
image.style.width = '400px';
image.style.height = '400px';
image.style.border = '1px solid red';
image.style.position = 'absolute';
image.style.left = '0';
image.style.top = '0';

area = document.createElement('area');
area.setAttribute('href', '#');
area.setAttribute('onclick', 'areaClicked = true; return false;');

areaClicked = false;

var map = document.createElement('map');
map.setAttribute('name', 'm');
map.appendChild(area);

document.body.appendChild(image);
document.body.appendChild(map);

function setArea(shape, coords)
{
    area.setAttribute('shape', shape);
    area.setAttribute('coords', coords);
}

function checkForArea(x, y)
{
    if (!window.eventSender)
        return false;

    areaClicked = false;

    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();

    return areaClicked;
}

shouldBe("setArea('default', ''); checkForArea(50, 50)", "true");

shouldBe("setArea('rect', '0, 0, 100, 100'); checkForArea(50, 50)", "true");
shouldBe("setArea('rect', '0, 0, 100, 100'); checkForArea(150, 150)", "false");

shouldBe("setArea('rect', '200, 200, 300, 300'); checkForArea(50, 50)", "false");
shouldBe("setArea('rect', '200, 200, 300, 300'); checkForArea(250, 250)", "true");

shouldBe("setArea('circle', '100, 100, 50'); checkForArea(100, 100)", "true");
shouldBe("setArea('circle', '100, 100, 50'); checkForArea(120, 100)", "true");
shouldBe("setArea('circle', '100, 100, 50'); checkForArea(200, 100)", "false");

shouldBe("setArea('circle', '300, 300, 50'); checkForArea(100, 100)", "false");
shouldBe("setArea('circle', '300, 300, 50'); checkForArea(300, 300)", "true");
shouldBe("setArea('circle', '300, 300, 50'); checkForArea(320, 300)", "true");

shouldBe("setArea('poly', '100, 100, 200, 100, 200, 200'); checkForArea(150, 150)", "true");
shouldBe("setArea('poly', '100, 100, 200, 100, 200, 200'); checkForArea(100, 150)", "false");
shouldBe("setArea('poly', '100, 100, 200, 100, 200, 200'); checkForArea(300, 300)", "false");

shouldBe("setArea('default', ''); checkForArea(300, 300)", "true");

document.body.removeChild(image);
