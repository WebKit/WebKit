description("This tests the SVG path parser by parsing and then re-serializing various paths.");

var pathElement = document.createElementNS("http://www.w3.org/2000/svg", "path");

var pathProperties = {
    "M": [ "x", "y" ],
    "m": [ "x", "y" ],
    "L": [ "x", "y" ],
    "l": [ "x", "y" ],
    "H": [ "x" ],
    "h": [ "x" ],
    "V": [ "y" ],
    "v": [ "y" ],
    "Z": [ ],
    "z": [ ],
    "C": [ "x1", "y1", "x2", "y2", "x", "y" ],
    "c": [ "x1", "y1", "x2", "y2", "x", "y" ],
    "S": [ "x2", "y2", "x", "y" ],
    "s": [ "x2", "y2", "x", "y" ],
    "Q": [ "x1", "y1", "x", "y" ],
    "q": [ "x1", "y1", "x", "y" ],
    "T": [ "x", "y" ],
    "t": [ "x", "y" ],
    "A": [ "r1", "r2", "angle", "largeArcFlag", "sweepFlag", "x", "y" ],
    "a": [ "r1", "r2", "angle", "largeArcFlag", "sweepFlag", "x", "y" ]
};

function printSegment(segment)
{
    var letter = segment.pathSegTypeAsLetter;
    var names = pathProperties[letter];
    if (!names)
        return letter + "?";
    var string = letter;
    for (var i = 0; i < names.length; ++i) {
        if (i)
            string += ",";
        var value = segment[names[i]];
        if (value == undefined) {
            string += "?";
            continue;
        }
        if (typeof(value) === "boolean") {
            string += value ? 1 : 0;
            continue;
        }
        string += value.toFixed(1).replace(/\.0$/, "");
    }
    return string;
}

function parsePath(string)
{
    pathElement.setAttributeNS(null, "d", string);

    var pathSegList = pathElement.pathSegList;
    var numberOfItems = pathSegList.numberOfItems;
    
    var pathCommands = "";
    for (var i = 0; i < numberOfItems; i++) {
        if (i)
            pathCommands += " ";
        pathCommands += printSegment(pathSegList.getItem(i));
    }

    return pathCommands;
}

shouldBe("parsePath('M1,2')", "'M1,2'");
shouldBe("parsePath('m1,2')", "'m1,2'");
shouldBe("parsePath('M100,200 m3,4')", "'M100,200 m3,4'");
shouldBe("parsePath('M100,200 L3,4')", "'M100,200 L3,4'");
shouldBe("parsePath('M100,200 l3,4')", "'M100,200 l3,4'");
shouldBe("parsePath('M100,200 H3')", "'M100,200 H3'");
shouldBe("parsePath('M100,200 h3')", "'M100,200 h3'");
shouldBe("parsePath('M100,200 V3')", "'M100,200 V3'");
shouldBe("parsePath('M100,200 v3')", "'M100,200 v3'");
shouldBe("parsePath('M100,200 Z')", "'M100,200 Z'");
shouldBe("parsePath('M100,200 z')", "'M100,200 Z'");
shouldBe("parsePath('M100,200 C3,4,5,6,7,8')", "'M100,200 C3,4,5,6,7,8'");
shouldBe("parsePath('M100,200 c3,4,5,6,7,8')", "'M100,200 c3,4,5,6,7,8'");
shouldBe("parsePath('M100,200 S3,4,5,6')", "'M100,200 S3,4,5,6'");
shouldBe("parsePath('M100,200 s3,4,5,6')", "'M100,200 s3,4,5,6'");
shouldBe("parsePath('M100,200 Q3,4,5,6')", "'M100,200 Q3,4,5,6'");
shouldBe("parsePath('M100,200 q3,4,5,6')", "'M100,200 q3,4,5,6'");
shouldBe("parsePath('M100,200 T3,4')", "'M100,200 T3,4'");
shouldBe("parsePath('M100,200 t3,4')", "'M100,200 t3,4'");
shouldBe("parsePath('M100,200 A3,4,5,0,0,6,7')", "'M100,200 A3,4,5,0,0,6,7'");
shouldBe("parsePath('M100,200 A3,4,5,1,0,6,7')", "'M100,200 A3,4,5,1,0,6,7'");
shouldBe("parsePath('M100,200 A3,4,5,0,1,6,7')", "'M100,200 A3,4,5,0,1,6,7'");
shouldBe("parsePath('M100,200 A3,4,5,1,1,6,7')", "'M100,200 A3,4,5,1,1,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,0,0,6,7')", "'M100,200 a3,4,5,0,0,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,0,1,6,7')", "'M100,200 a3,4,5,0,1,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,1,0,6,7')", "'M100,200 a3,4,5,1,0,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,1,1,6,7')", "'M100,200 a3,4,5,1,1,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,006,7')", "'M100,200 a3,4,5,0,0,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,016,7')", "'M100,200 a3,4,5,0,1,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,106,7')", "'M100,200 a3,4,5,1,0,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,116,7')", "'M100,200 a3,4,5,1,1,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,2,1,6,7')", "'M100,200'");
shouldBe("parsePath('M100,200 a3,4,5,1,2,6,7')", "'M100,200'");

// FIXME: This uses 'If rx = 0 or ry = 0 then this arc is treated as a straight line segment (a "lineto") joining the endpoints.'
// I think the SVG DOM should still show the arc segment, fix that!
shouldBe("parsePath('M100,200 a0,4,5,0,0,10,0 a4,0,5,0,0,0,10 a0,0,5,0,0,-10,0 z')", "'M100,200 l10,0 l0,10 l-10,0 Z'");

shouldBe("parsePath('M1,2,3,4')", "'M1,2 L3,4'");
shouldBe("parsePath('m100,200,3,4')", "'m100,200 l3,4'");

shouldBe("parsePath('M 100-200')", "'M100,-200'");
shouldBe("parsePath('M 0.6.5')", "'M0.6,0.5'");

shouldBe("parsePath(' M1,2')", "'M1,2'");
shouldBe("parsePath('  M1,2')", "'M1,2'");
shouldBe("parsePath('\\tM1,2')", "'M1,2'");
shouldBe("parsePath('\\nM1,2')", "'M1,2'");
shouldBe("parsePath('\\rM1,2')", "'M1,2'");
shouldBe("parsePath('\\vM1,2')", "''");
shouldBe("parsePath('xM1,2')", "''");
shouldBe("parsePath('M1,2 ')", "'M1,2'");
shouldBe("parsePath('M1,2\\t')", "'M1,2'");
shouldBe("parsePath('M1,2\\n')", "'M1,2'");
shouldBe("parsePath('M1,2\\r')", "'M1,2'");
shouldBe("parsePath('M1,2\\v')", "'M1,2'");
shouldBe("parsePath('M1,2x')", "'M1,2'");
shouldBe("parsePath('M1,2 L40,0#90')", "'M1,2 L40,0'");

shouldBe("parsePath('')", "''");
shouldBe("parsePath(' ')", "''");
shouldBe("parsePath('x')", "''");
shouldBe("parsePath('L1,2')", "''");
shouldBe("parsePath('M.1 .2 L.3 .4 .5 .6')", "'M0.1,0.2 L0.3,0.4 L0.5,0.6'");

successfullyParsed = true;
