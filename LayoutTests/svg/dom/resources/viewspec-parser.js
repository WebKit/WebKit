description("This test fuzzes the transform parser with semi-random attribute values and dumps the results of any values that parse successfully.");

var attributes = { // maps a viewspec attribute to its minimum argument count
    viewBox: 6,
    preserveAspectRatio: 1,
    transform: 1,
    zoomAndPan: 1,
    viewTarget: 1
};

var preserveAspectRatioValues = [ "Min", "Max", "Mid" ];

var characters = [
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    ".",
    "e",
    "+",
    "-",
    "e",
    "(",
    ")",
    " ", // not a valid fragment char
    "\t", // not a valid fragment char
    ","
];

function buildTestsToRun()
{
  for (var attribute in attributes) {

      // Too few / too many arguments
      for (var i = 0; i < 5; i++) { //>
          var attributeString = "svgView(" + attribute + "(";
          for (var j = 0; j < i; j++) { //>
              attributeString += "0";
              if (j < i - 1) //>
                  attributeString += ",";
          }
          attributeString += "))";
          testsToRun.push(attributeString);
      }

      // Random assortments of valid characters
      for (var i = 0; i < 50; i++) { //>
          var attributeString = "svgView(" + attribute + "(";
          var count = Math.scriptedRandomInt(20);
          for (var j = 0; j < count; j++) { //>
              attributeString += characters[Math.scriptedRandomInt(characters.length)];
          }
          testsToRun.push(attributeString);
      }

      // attribute names that are "off by one"
      var extraChar = attribute.charAt(attribute.length - 1);
      testsToRun.push("svgView(" + attribute + extraChar + "(0, 0)");
      testsToRun.push("svgView(" + attribute.substring(0, attribute.length - 1) + "(0, 0)");

      // Empty-ish attributes
      testsToRun.push("svgView(" + attribute);
      testsToRun.push("svgView(" + attribute + String.fromCharCode(0));
  }
}

function zoomAndPanToString(zoomAndPan)
{
    if (zoomAndPan == SVGZoomAndPan.SVG_ZOOMANDPAN_MAGNIFY)
        return "magnify";
    if (zoomAndPan == SVGZoomAndPan.SVG_ZOOMANDPAN_DISABLE)
        return "disable";
    return "unknown";
}

function viewSpecToString(viewSpec)
{
    if (!viewSpec)
        return undefined;

    var attributes = [];
    if (viewSpec.transformString)
        attributes.push("transform(" + viewSpec.transformString + ")");
    if (viewSpec.viewBoxString && viewSpec.viewBoxString != "0 0 0 0")
        attributes.push("viewBox(" + viewSpec.viewBoxString + ")");
    if (viewSpec.preserveAspectRatioString && viewSpec.preserveAspectRatioString != "xMidYMid meet")
        attributes.push("preserveAspectRatio(" + viewSpec.preserveAspectRatioString + ")");
    if (viewSpec.zoomAndPan && viewSpec.zoomAndPan != SVGZoomAndPan.SVG_ZOOMANDPAN_MAGNIFY)
        attributes.push("zoomAndPan(" + zoomAndPanToString(viewSpec.zoomAndPan) + ")");
    if (viewSpec.viewTargetString)
        attributes.push("viewTarget(" + viewSpec.viewTargetString + ")");
    if (!attributes.length)
        return "[initial view]";
    return "svgView(" + attributes.join(";") + ")";
}

var testNumber = 0;
var testString = "[initial view]"

function makeURLRelative(url)
{
    return url.slice(url.indexOf("resources"));
}

function testFragment(string)
{
    var oldEmbed = document.getElementById("object");
    if (oldEmbed)
        oldEmbed.parentNode.removeChild(oldEmbed);
    var embedElement = document.createElement("iframe");
    embedElement.setAttribute("id", "object");
    embedElement.setAttribute("width", "100");
    embedElement.setAttribute("height", "100");
    embedElement.setAttribute("onload", "setTimeout('continueFuzzing(event)', 0)");
    var newURL = "resources/viewspec-target.svg#" + string;
    embedElement.src = newURL;
    document.body.appendChild(embedElement);
}

function startNextTest()
{
    testFragment(testString);
}

function continueFuzzing(event)
{
    var embedElement = document.getElementById("object");
    if (embedElement.contentDocument) {
        debug("Loaded: " + makeURLRelative(embedElement.contentDocument.URL));
        debug("Parsed: " + viewSpecToString(embedElement.contentDocument.documentElement.currentView) + " from: " + testString + "\n");
    } else
        debug("no svgdocument");

    if (testNumber < testsToRun.length)
        testString = testsToRun[testNumber];
    else {
        var script = document.createElement("script");

        script.onload = function() {
            if (window.testRunner)
                testRunner.notifyDone();
        };

        script.src = "../../fast/js/resources/js-test-post.js";
        document.body.appendChild(script);
        return;
    }
    testNumber++;

    // this lets us out of the onload handler so we don't overrun the stack
    window.setTimeout(startNextTest, 0);
}

if (window.testRunner)
    testRunner.waitUntilDone();

buildTestsToRun();
testFragment("");
