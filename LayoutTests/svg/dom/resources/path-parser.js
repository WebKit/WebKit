description("This tests the SVG path parser by parsing and then re-serializing various paths.");

var pathElement = document.createElementNS("http://www.w3.org/2000/svg", "path");

var pathProperties = {
    "M": [ "x", "y" ],
    "L": [ "x", "y" ],
    "H": [ "x" ],
    "V": [ "y" ],
    "Z": [ ],
    "C": [ "x1", "y1", "x2", "y2", "x", "y" ],
    "S": [ "x2", "y2", "x", "y" ],
    "Q": [ "x1", "y1", "x", "y" ],
    "T": [ "x", "y" ],
    "A": [ "r1", "r2", "angle", "largeArcFlag", "sweepFlag", "x", "y" ],
};

function printSegment(segment)
{
    var letter = segment.pathSegTypeAsLetter;
    var names = pathProperties[letter.toUpperCase()];
    if (!names)
        return letter + "?";
    var string = letter;
    for (var i = 0; i < names.length; ++i) {
        if (i)
            string += ",";
        var value = segment[names[i]];
        if (!value) {
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
shouldBe("parsePath('m1,2')", "'M1,2'");
shouldBe("parsePath('M100,200 m3,4')", "'M100,200 M103,204'");
shouldBe("parsePath('M100,200 L3,4')", "'M100,200 L3,4'");
shouldBe("parsePath('M100,200 l3,4')", "'M100,200 L103,204'");
shouldBe("parsePath('M100,200 H3')", "'M100,200 L3,200'");
shouldBe("parsePath('M100,200 h3')", "'M100,200 L103,200'");
shouldBe("parsePath('M100,200 V3')", "'M100,200 L100,3'");
shouldBe("parsePath('M100,200 v3')", "'M100,200 L100,203'");
shouldBe("parsePath('M100,200 Z')", "'M100,200 Z'");
shouldBe("parsePath('M100,200 z')", "'M100,200 Z'");
shouldBe("parsePath('M100,200 C3,4,5,6,7,8')", "'M100,200 C3,4,5,6,7,8'");
shouldBe("parsePath('M100,200 c3,4,5,6,7,8')", "'M100,200 C103,204,105,206,107,208'");
shouldBe("parsePath('M100,200 S3,4,5,6')", "'M100,200 C100,200,3,4,5,6'");
shouldBe("parsePath('M100,200 s3,4,5,6')", "'M100,200 C100,200,103,204,105,206'");
shouldBe("parsePath('M100,200 Q3,4,5,6')", "'M100,200 C35.3,69.3,3.7,4.7,5,6'");
shouldBe("parsePath('M100,200 q3,4,5,6')", "'M100,200 C102,202.7,103.7,204.7,105,206'");
shouldBe("parsePath('M100,200 T3,4')", "'M100,200 C100,200,67.7,134.7,3,4'");
shouldBe("parsePath('M100,200 t3,4')", "'M100,200 C100,200,101,201.3,103,204'");
shouldBe("parsePath('M100,200 A3,4,5,0,0,6,7')", "'M100,200 C141.5,162.8,154.1,89.4,128.2,36.2 C102.2,-17.1,47.5,-30.2,6,7'");
shouldBe("parsePath('M100,200 A3,4,5,1,0,6,7')", "'M100,200 C141.5,162.8,154.1,89.4,128.2,36.2 C102.2,-17.1,47.5,-30.2,6,7'");
shouldBe("parsePath('M100,200 A3,4,5,0,1,6,7')", "'M100,200 C58.5,237.2,3.8,224.1,-22.2,170.8 C-48.1,117.6,-35.5,44.2,6,7'");
shouldBe("parsePath('M100,200 A3,4,5,1,1,6,7')", "'M100,200 C58.5,237.2,3.8,224.1,-22.2,170.8 C-48.1,117.6,-35.5,44.2,6,7'");
shouldBe("parsePath('M100,200 a3,4,5,0,0,6,7')", "'M100,200 C98.5,202.3,98.6,205.7,100.2,207.7 C101.9,209.6,104.5,209.3,106,207'");
shouldBe("parsePath('M100,200 a3,4,5,0,1,6,7')", "'M100,200 C101.5,197.7,104.1,197.4,105.8,199.3 C107.4,201.3,107.5,204.7,106,207'");
shouldBe("parsePath('M100,200 a3,4,5,1,0,6,7')", "'M100,200 C98.5,202.3,98.6,205.7,100.2,207.7 C101.9,209.6,104.5,209.3,106,207'");
shouldBe("parsePath('M100,200 a3,4,5,1,1,6,7')", "'M100,200 C101.5,197.7,104.1,197.4,105.8,199.3 C107.4,201.3,107.5,204.7,106,207'");

shouldBe("parsePath('M1,2,3,4')", "'M1,2 L3,4'");
shouldBe("parsePath('m100,200,3,4')", "'M100,200 L103,204'");

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
shouldBe("parsePath('M1,2\\v')", "''");
shouldBe("parsePath('M1,2x')", "''");

shouldBe("parsePath('')", "''");
shouldBe("parsePath('x')", "''");
shouldBe("parsePath('L1,2')", "''");
shouldBe("parsePath('M.1 .2 L.3 .4 .5 .6')", "'M0.1,0.2 L0.3,0.4 L0.5,0.6'");

successfullyParsed = true;
