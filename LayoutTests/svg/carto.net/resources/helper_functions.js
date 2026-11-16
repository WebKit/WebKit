/*
ECMAScript helper functions
Copyright (C) <2004>  <Andreas Neumann>
Version 1.1, 2004-11-18
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits: numerous people on svgdevelopers@yahoogroups.com

This ECMA script library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library (http://www.carto.net/papers/svg/resources/lesser_gpl.txt); if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

----

original document site: http://www.carto.net/papers/svg/resources/helper_functions.js
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use these scripts, please link to the original (http://www.carto.net/papers/svg/navigationTools/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

//global variables necessary to create elements in these namespaces, do not delete them!!!!
var svgNS = "http://www.w3.org/2000/svg";
var xlinkNS = "http://www.w3.org/1999/xlink";
var cartoNS = "http://www.carto.net/attrib";
var attribNS = "http://www.carto.net/attrib";
var batikNS = "http://xml.apache.org/batik/ext";

/* ----------------------- helper functions to calculate stuff ---------------- */
/* ---------------------------------------------------------------------------- */
function toPolarDir(xdiff,ydiff) { // Subroutine for calculating polar Coordinates
   direction = (Math.atan2(ydiff,xdiff));
   //result is angle in radian
   return(direction);
}

function toPolarDist(xdiff,ydiff) { // Subroutine for calculating polar Coordinates
   distance = Math.sqrt(xdiff * xdiff + ydiff * ydiff);
   return(distance);
}

function toRectX(direction,distance) { // Subroutine for calculating cartesic coordinates
   x = distance * Math.cos(direction);
   y = distance * Math.sin(direction);
   return(x);
}

function toRectY(direction,distance) { // Subroutine for calculating cartesic coordinates
   x = distance * Math.cos(direction);
   y = distance * Math.sin(direction);
   return(y);
}

//Converts degrees to radians.
function DegToRad(deg) {
     return (deg / 180.0 * Math.PI);
}

//Converts radians to degrees.
function RadToDeg(rad) {
     return (rad / Math.PI * 180.0);
}

//converts decimal degrees to degree/minutes/seconds
function dd2dms(dd) {
        var minutes = (Math.abs(dd) - Math.floor(Math.abs(dd))) * 60;
        var seconds = (minutes - Math.floor(minutes)) * 60;
        var minutes = Math.floor(minutes);
        var degrees = Math.floor(dd);
        return {deg:degrees,min:minutes,sec:seconds};
}

//converts degree/minutes/seconds to decimal degrees
function dms2dd(deg,min,sec) {
        return deg + (min / 60) + (sec / 3600);
}

//log functions that do not exist in Math object
function log(x,b) {
    if(b==null) b=Math.E;
    return Math.log(x)/Math.log(b);
}

//gets 4 z-values (4 corners), a position, delta x and delty and a cellsize as input and returns interpolated z-value
function intBilinear(za,zb,zc,zd,xpos,ypos,ax,ay,cellsize) { //bilinear interpolation function
    e = (xpos - ax) / cellsize;
    f = (ypos - ay) / cellsize;

    //calculation of weights
    wa = (1 - e) * (1 - f);
    wb = e * (1 - f);
    wc = e * f;
    wd = f * (1 - e);

    height_interpol = wa * zc + wb * zd + wc * za + wd * zb;

    return (height_interpol);    
}

//test if point is left of or right of, result is 1 (leftof) or 0 (rightof)
function leftOfTest(pointx,pointy,linex1,liney1,linex2,liney2) {
    result = (liney1 - pointy) * (linex2 - linex1) - (linex1 - pointx) * (liney2 - liney1);
    if (result < 0) {
        leftof = 1; //case left of
    }
    else {
        leftof = 0; //case left of    
    }
    return(leftof);
}

//input is point coordinate, and 2 line coordinates
function distFromLine(xpoint,ypoint,linex1,liney1,linex2,liney2) {
    dx = linex2 - linex1;
    dy = liney2 - liney1;
    distance = (dy * (xpoint - linex1) - dx * (ypoint - liney1)) / Math.sqrt(Math.pow(dx,2) + Math.pow(dy,2));
    return(distance);
}

//converts radian value to degrees
function radian2deg(radian) {
    deg = radian / Math.PI * 180;
    return(deg);
}

//input is two vectors (a1,a2 is vector a, b1,b2 is vector b), output is angle in radian
//Formula: Acos from  Scalaproduct of the two vectors divided by ( norm (deutsch Betrag) vector 1 by norm vector 2
//see http://www.mathe-online.at/mathint/vect2/i.html#Winkel
function angleBetwTwoLines(a1,a2,b1,b2) {
    angle = Math.acos((a1 * b1 + a2 * b2) / (Math.sqrt(Math.pow(a1,2) + Math.pow(a2,2)) * Math.sqrt(Math.pow(b1,2) + Math.pow(b2,2))));
    return(angle);
}

//input is two vectors (a1,a2 is vector a, b1,b2 is vector b), output is new vector c2 returned as array
//Formula: Vektor a divided by Norm Vector a (Betrag) plus Vektor b divided by Norm Vector b (Betrag)
//see http://www.mathe-online.at/mathint/vect1/i.html#Winkelsymmetrale
function calcBisectorVector(a1,a2,b1,b2) {
    betraga = Math.sqrt(Math.pow(a1,2) + Math.pow(a2,2));
    betragb = Math.sqrt(Math.pow(b1,2) + Math.pow(b2,2));
    c = new Array();
    c[0] = a1 / betraga + b1 / betragb;
    c[1] = a2 / betraga + b2 / betragb;
    return(c);
}

//input is two vectors (a1,a2 is vector a, b1,b2 is vector b), output is angle in radian
//Formula: Vektor a divided by Norm Vector a (Betrag) plus Vektor b divided by Norm Vector b (Betrag)
//see http://www.mathe-online.at/mathint/vect1/i.html#Winkelsymmetrale
function calcBisectorAngle(a1,a2,b1,b2) {
    betraga = Math.sqrt(Math.pow(a1,2) + Math.pow(a2,2));
    betragb = Math.sqrt(Math.pow(b1,2) + Math.pow(b2,2));
    c1 = a1 / betraga + b1 / betragb;
    c2 = a2 / betraga + b2 / betragb;
    angle = toPolarDir(c1,c2);
    return(angle);
}

function intersect2lines(line1x1,line1y1,line1x2,line1y2,line2x1,line2y1,line2x2,line2y2) {
    //formula see http://astronomy.swin.edu.au/~pbourke/geometry/lineline2d/
    var result = new Array();
    var denominator = (line2y2 - line2y1)*(line1x2 - line1x1) - (line2x2 - line2x1)*(line1y2 - line1y1);
    if (denominator == 0) {
        alert("lines are parallel");
    }
    else {
        ua = ((line2x2 - line2x1)*(line1y1 - line2y1) - (line2y2 - line2y1)*(line1x1 - line2x1)) / denominator;
        ub = ((line1x2 - line1x1)*(line1y1 - line2y1) - (line1y2 - line1y1)*(line1x1 - line2x1)) / denominator;
    }
    result["x"] = line1x1 + ua * (line1x2 - line1x1);
    result["y"] = line1y1 + ua * (line1y2 - line1y1);
    return(result);
}

/* ----------------------- helper function to sort arrays ---------------- */
/* ----------------------------------------------------------------------- */
//my own sort function, uses only first part of string (population value)
function mySort(a,b) {
    var myResulta = a.split("+");
    var myResultb = b.split("+");
    if (parseFloat(myResulta[0]) < parseFloat(myResultb[0])) {
        return 1;
    }
    else {
        return -1;
    }
}

/* ----------------------- helper function format number strings -------------- */
/* ---------------------------------------------------------------------------- */
//formatting number strings
//this function add's "'" to a number every third digit
function formatNumberString(myString) {
    //check if of type string, if number, convert it to string
    if (typeof(myString) == "number") {
        myTempString = myString.toString();
    }
    else {
        myTempString = myString;
    }
    var myNewString="";
    //if it contains a comma, it will be split
    var splitResults = myTempString.split(".");
    var myCounter= splitResults[0].length;
    if (myCounter > 3) {
        while(myCounter > 0) {
            if (myCounter > 3) {
                myNewString = "," + splitResults[0].substr(myCounter - 3,3) + myNewString;
            }
            else {
                myNewString = splitResults[0].substr(0,myCounter) + myNewString;
            }
            myCounter -= 3;
        }
    }
    else {
        myNewString = splitResults[0];
    }
    //concatenate if it contains a comma
    if (splitResults[1]) {
        myNewString = myNewString + "." + splitResults[1];
    }
    return myNewString;
}

//function for status Bar
function statusChange(statusText) {
    document.getElementById("statusText").firstChild.nodeValue = "Statusbar: " + statusText;
}

//scale an object
function scaleObject(evt,factor) {
//reference to the currently selected object
    var element = evt.currentTarget;
    var myX = element.getAttributeNS(null,"x");
    var myY = element.getAttributeNS(null,"y");
    var newtransform = "scale(" + factor + ") translate(" + (myX * 1 / factor - myX) + " " + (myY * 1 / factor - myY) +")";
    element.setAttributeNS(null,'transform', newtransform);
}

//this code is copied from Kevin Lindsey
//http://www.kevlindev.com/tutorials/basics/transformations/toUserSpace/index.htm
function getTransformToRootElement(node) {
     try {
        //this part is for fully conformant players
        var CTM = node.getTransformToElement(document.documentElement);
    }
    catch (ex) {
        //this part is for ASV3 or other non-conformant players
        // Initialize our CTM the node's Current Transformation Matrix
        var CTM = node.getCTM();
        // Work our way through the ancestor nodes stopping at the SVG Document
        while ( ( node = node.parentNode ) != document ) {
            // Multiply the new CTM to the one with what we have accumulated so far
            CTM = node.getCTM().multiply(CTM);
        }
    }
    return CTM;
}

//this is because ASV does not implement the method SVGLocatable.getTransformToElement()
function getTransformToElement(node,targetNode) {
    try {
        //this part is for fully conformant players
        var CTM = node.getTransformToElement(targetNode);
    }
    catch (ex) {
          //this part is for ASV3 or other non-conformant players
        // Initialize our CTM the node's Current Transformation Matrix
        var CTM = node.getCTM();
        // Work our way through the ancestor nodes stopping at the SVG Document
        while ( ( node = node.parentNode ) != targetNode ) {
            // Multiply the new CTM to the one with what we have accumulated so far
            CTM = node.getCTM().multiply(CTM);
        }
    }
    return CTM;
}

//calculate HSV 2 RGB: HSV (h 0 to 360, sat and val are between 0 and 1), RGB between 0 and 255
function hsv2rgb(hue,sat,val) {
    //alert("Hue:"+hue);
    var rgbArr = new Array();
    if ( sat == 0) {
        rgbArr["red"] = Math.round(val * 255);
        rgbArr["green"] = Math.round(val * 255);
        rgbArr["blue"] = Math.round(val * 255);
    }
    else {
        var h = hue / 60;
        var i = Math.floor(h);
        var f = h - i;
        if (i % 2 == 0) {
            f = 1 - f;
        }
        var m = val * (1 - sat); 
        var n = val * (1 - sat * f);
        switch(i) {
            case 0:
                rgbArr["red"] = val;
                rgbArr["green"] = n;
                rgbArr["blue"] = m;
                break;
            case 1:
                rgbArr["red"] = n;
                rgbArr["green"] = val;
                rgbArr["blue"] = m;
                break;
            case 2:
                rgbArr["red"] = m;
                rgbArr["green"] = val;
                rgbArr["blue"] = n;
                break;
            case 3:
                rgbArr["red"] = m;
                rgbArr["green"] = n;
                rgbArr["blue"] = val;
                break;
            case 4:
                rgbArr["red"] = n;
                rgbArr["green"] = m;
                rgbArr["blue"] = val;
                break;
            case 5:
                rgbArr["red"] = val;
                rgbArr["green"] = m;
                rgbArr["blue"] = n;
                break;
            case 6:
                rgbArr["red"] = val;
                rgbArr["green"] = n;
                rgbArr["blue"] = m;
                break;
        }
        rgbArr["red"] = Math.round(rgbArr["red"] * 255);
        rgbArr["green"] = Math.round(rgbArr["green"] * 255);
        rgbArr["blue"] = Math.round(rgbArr["blue"] * 255);
    }
    return rgbArr;
}

//calculate rgb to hsv values
function rgb2hsv (red,green,blue) {
    //input between 0 and 255 --> normalize to 0 to 1
    //result = 
    var hsvArr = new Array();
    red = red / 255;
    green = green / 255;
    blue = blue / 255;
    myMax = Math.max(red, Math.max(green,blue));
    myMin = Math.min(red, Math.min(green,blue));
    v = myMax;
    if (myMax > 0) {
        s = (myMax - myMin) / myMax;
    }
    else {
        s = 0;
    }
    if (s > 0) {
        myDiff = myMax - myMin;
        rc = (myMax - red) / myDiff;
        gc = (myMax - green) / myDiff;
        bc = (myMax - blue) / myDiff;
        if (red == myMax) {
            h = (bc - gc) / 6;
        }
        if (green == myMax) {
            h = (2 + rc - bc) / 6;
        }
        if (blue == myMax) {
            h = (4 + gc - rc) / 6;
        }
    }
    else {
        h = 0;
    }
    if (h < 0) {
        h += 1;
    }
    hsvArr["hue"] = Math.round(h * 360);
    hsvArr["sat"] = s;
    hsvArr["val"] = v;
    return hsvArr;
}

//populate an array that can be addressed by both a key or an index nr
function assArrayPopulate(arrayKeys,arrayValues) {
    var returnArray = new Array();
    if (arrayKeys.length != arrayValues.length) {
        alert("error: arrays do not have same length!");
    }
    else {
        for (i=0;i<arrayKeys.length;i++) {
            returnArray[arrayKeys[i]] = arrayValues[i];
        }
    }
    return returnArray;
}

//replace special (non-ASCII) characters with their charCode
function replaceSpecialChars(myString) {
        for (i=161;i<256;i++) {
            re = new RegExp("&#"+i+";","g");
            myString = myString.replace(re,String.fromCharCode(i));
        }
        return myString;
}

/* ----------------------- getXMLData object ----------------------------- */
/* ----------------------------------------------------------------------- */
//this object allows to make network requests using getURL or XMLHttpRequest
//you may specify a url and a callBackFunction
//the callBackFunction receives a XML node representing the rootElement of the fragment received
function getXMLData(url,callBackFunction) {
    this.url = url;
    this.callBackFunction = callBackFunction;
    this.xmlRequest = null;
} 

getXMLData.prototype.getData = function() {
    //call getURL() if available
    if (window.getURL) {
        getURL(this.url,this);
    }
    //or call XMLHttpRequest() if available
    else if (window.XMLHttpRequest) {
        var _this = this;
        this.xmlRequest = new XMLHttpRequest();
        this.xmlRequest.overrideMimeType("text/xml");
        this.xmlRequest.open("GET",this.url,true);
        this.xmlRequest.onreadystatechange = function() {_this.handleEvent()};
        this.xmlRequest.send(null);
    }
    //write an error message if neither method is available
    else {
        alert("your browser/svg viewer neither supports window.getURL nor window.XMLHttpRequest!");
    }    
}

//this is the callback method for the getURL function
getXMLData.prototype.operationComplete = function(data) {
    //check if data has a success property
    if (data.success) {
        //parse content of the XML format to the variable "node"
        var node = parseXML(data.content,document);
        this.callBackFunction(node.firstChild);
    }
    else {
        alert("something went wrong with dynamic loading of geometry!");
    }
}

//this method receives data from XMLHttpRequest
getXMLData.prototype.handleEvent = function() {
    if (this.xmlRequest.readyState == 4) {
        var importedNode = document.importNode(this.xmlRequest.responseXML.documentElement,true);
        this.callBackFunction(importedNode);
    }    
}

//starts an animation with the given id
//this function is useful in combination with window.setTimeout()
function startAnimation(id) {
        document.getElementById(id).beginElement();
}
