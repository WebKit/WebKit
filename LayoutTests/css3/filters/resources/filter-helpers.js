
function buildCSSFilteredBoxes(filterValues)
{
    for (const value of filterValues) {
        const img = new Image();
        img.src = 'resources/reference.png';
        img.setAttribute('style', `filter: ${value}`);
        document.body.appendChild(img);
    }
}

function buildSVGFilteredBoxes(filterValues)
{
    const filterIDs = buildSVGFilters(filterValues);

    for (const filterID of filterIDs) {
        const img = new Image();
        img.src = 'resources/reference.png';
        img.setAttribute('style', `filter: url('#${filterID}');`);
        document.body.appendChild(img);
    }
}

const filterNameToFunctionMap = {
    'brightness' : svgBrightnessComponent,
    'contrast' : svgContrastComponent,
    'grayscale' : svgGrayscaleComponent,
    'sepia' : svgSepiaComponent,
    'saturate' : svgSaturateComponent,
    'hue-rotate' : svgHueRotateComponent,
    'invert' : svgInvertComponent,
    'opacity' : svgOpacityComponent,
    'blur' : svgBlurComponent,
    'drop-shadow' : svgDropShadowComponent,
};

function buildSVGFilters(filterValues)
{
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    let filterIDs = [];
    
    let filterIndex = 0;
    for (const value of filterValues) {

        let filterElement = document.createElementNS('http://www.w3.org/2000/svg', 'filter');
        
        const filterFunctionPattern = /[-a-zA-Z]+\(.+?\)/g; // Doesn't support nested parens.
        const filterList = [...value.matchAll(filterFunctionPattern)];

        for (const filterString of filterList) {
            const filterPattern = /(.+)\((.*)\)/;
            const matchResult = filterPattern.exec(filterString);
            const filterType = matchResult[1];
            const param = matchResult[2];

            const filterCreationFunction = filterNameToFunctionMap[filterType];
            if (!filterCreationFunction) {
                console.log(`Failed to create filter of type ${filterType}`);
                continue;
            }

            if (filterType === 'drop-shadow') {
                // Because the SVG equivalent of drop-shadow involves multiple components,
                // we can't support it in a list.
                filterElement = filterCreationFunction(param);
                break;
            } else {
                const filterComponent = filterCreationFunction(param);
                filterElement.appendChild(filterComponent);
            }
        }

        const filterID = `filter-${++filterIndex}`;
        filterElement.id = filterID;
        filterIDs.push(filterID);
        svg.appendChild(filterElement);
    }

    document.body.appendChild(svg);    
    return filterIDs;
}

function elementFromHTMLString(s)
{
    const filterElement = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    filterElement.innerHTML = s;
    return filterElement.firstElementChild;
}

function svgBrightnessComponent(filterValue)
{
    return elementFromHTMLString(`<feComponentTransfer>
          <feFuncR type="linear" slope="${filterValue}"/>
          <feFuncG type="linear" slope="${filterValue}"/>
          <feFuncB type="linear" slope="${filterValue}"/>
      </feComponentTransfer>`);
}

function svgContrastComponent(filterValue)
{
    const slope = filterValue;
    const intercept =-(0.5 * filterValue) + 0.5;
    return elementFromHTMLString(`<feComponentTransfer>
            <feFuncR type="linear" slope="${slope}" intercept="${intercept}"/>
            <feFuncG type="linear" slope="${slope}" intercept="${intercept}"/>
            <feFuncB type="linear" slope="${slope}" intercept="${intercept}"/>
        </feComponentTransfer>`);
}

function svgGrayscaleComponent(filterValue)
{
    const oneMinusAmount = 1 - filterValue;
    const values = [
        (0.2126 + 0.7874 * oneMinusAmount), (0.7152 - 0.7152  * oneMinusAmount), (0.0722 - 0.0722 * oneMinusAmount), 0, 0,
        (0.2126 - 0.2126 * oneMinusAmount), (0.7152 + 0.2848  * oneMinusAmount), (0.0722 - 0.0722 * oneMinusAmount), 0, 0,
        (0.2126 - 0.2126 * oneMinusAmount), (0.7152 - 0.7152  * oneMinusAmount), (0.0722 + 0.9278 * oneMinusAmount), 0, 0,
        0, 0, 0, 1, 0        
    ];

    return elementFromHTMLString(`<feColorMatrix type="matrix" values="${values.join(' ')}"/>`);
}

function svgSepiaComponent(filterValue)
{
    const oneMinusAmount = 1 - filterValue;
    const values = [
        (0.393 + 0.607 * oneMinusAmount), (0.769 - 0.769 * oneMinusAmount), (0.189 - 0.189 * oneMinusAmount), 0, 0,
        (0.349 - 0.349 * oneMinusAmount), (0.686 + 0.314 * oneMinusAmount), (0.168 - 0.168 * oneMinusAmount), 0, 0,
        (0.272 - 0.272 * oneMinusAmount), (0.534 - 0.534 * oneMinusAmount), (0.131 + 0.869 * oneMinusAmount), 0, 0,
        0, 0, 0, 1, 0        
    ];

    return elementFromHTMLString(`<feColorMatrix type="matrix" values="${values.join(' ')}"/>`);
}

function svgSaturateComponent(filterValue)
{
    return elementFromHTMLString(`<feColorMatrix type="saturate" values="${filterValue}"/>`);
}

function svgHueRotateComponent(filterValue)
{
    const valuePattern = /(.+)turn/;
    const matchResult = valuePattern.exec(filterValue);
    if (!matchResult) {
        console.log('hue-rotate only accepts angles in turns');
        return null;
    }
    const angleInDegrees = 360 * matchResult[1];
    
    return elementFromHTMLString(`<feColorMatrix type="hueRotate" values="${angleInDegrees}"/>`);
}

function svgInvertComponent(filterValue)
{
    const oneMinusAmount = 1 - filterValue;
    return elementFromHTMLString(`<feComponentTransfer>
          <feFuncR type="table" tableValues="${filterValue} ${oneMinusAmount}"/>
          <feFuncG type="table" tableValues="${filterValue} ${oneMinusAmount}"/>
          <feFuncB type="table" tableValues="${filterValue} ${oneMinusAmount}"/>
        </feComponentTransfer>`);
}

function svgOpacityComponent(filterValue)
{
    const oneMinusAmount = 1 - filterValue;
    return elementFromHTMLString(`<feComponentTransfer>
          <feFuncA type="table" tableValues="0 ${filterValue}"/>
      </feComponentTransfer>`);
}

function svgBlurComponent(filterValue)
{
    const valueRegExp = /(.+)px/;
    const matchResult = valueRegExp.exec(filterValue);
    if (!matchResult) {
        console.log('blur only accepts px units');
        return null;
    }
    const stdDeviation = matchResult[1];
    
    return elementFromHTMLString(`<feGaussianBlur stdDeviation="${stdDeviation}"/>`);
}

function svgDropShadowComponent(filterValue)
{
    // drop-shadow() parameters are (x offset, y offset, blur, color).
    const valuePattern = /(.+)\s+(.+)\s+(.+)\s+(.+)/;
    const matchResult = valuePattern.exec(filterValue);
    if (!matchResult) {
        console.log(`Failed to match drop-shadow parameters ${filterValue}`);
        return null;
    }
    
    const xOffset = matchResult[1].replace(/(px)?$/, '');
    const yOffset = matchResult[2].replace(/(px)?$/, '');
    const stdDeviation = matchResult[3].replace(/(px)?$/, '');
    const color = matchResult[4];

    return elementFromHTMLString(`<filter>
        <feGaussianBlur in="SourceAlpha" stdDeviation="${stdDeviation}"/>
          <feOffset dx="${xOffset}" dy="${yOffset}" result="offsetblur"/>
          <feFlood flood-color="${color}"/>
          <feComposite in2="offsetblur" operator="in"/>
          <feMerge>
            <feMergeNode/>
            <feMergeNode in="SourceGraphic"/>
          </feMerge>
        </filter>`);
}
