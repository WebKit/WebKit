function createContainer(corner, colors, clipPath)
{
    var container = document.createElement('div');
    container.className = 'container';
    
    var testDiv = document.createElement('div');
    testDiv.className = 'test';
    
    var sides = corner.split('-');
    for (var index in sides)
        testDiv.style['border-' + sides[index]] = borderWidth + 'px solid ' + colors[index];

    testDiv.style['border-' + corner + '-radius'] = '100% 100%';
    testDiv.style.webkitClipPath = clipPath;
    
    container.appendChild(testDiv);

    return container;
}

function createReferenceContainer()
{
    var container = document.createElement('div');
    container.className = 'container';
    
    return container;
}
