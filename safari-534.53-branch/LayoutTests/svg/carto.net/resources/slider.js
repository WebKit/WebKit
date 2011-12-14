/*
Scripts to create interactive sliders in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 1.0, 2006-08-04
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:
* Kevin Lindsey for ideas how to implement such a slider

----

Documentation: http://www.carto.net/papers/svg/gui/slider/

----

current version: 1.0.

version history:
0.99 (not documented, somewhere around 2001)

1.0 (2006-08-04)
changed constructor parameters (added id, parentNode), removed parameter sliderGroupId, added documentation, added method .moveTo()

-------

This ECMA script library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library (lesser_gpl.txt); if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

----

original document site: http://www.carto.net/papers/svg/gui/slider/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/slider/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

//slider properties
function slider(id,parentNode,x1,y1,value1,x2,y2,value2,startVal,sliderStyles,invisSliderWidth,sliderSymb,functionToCall,mouseMoveBool) {
    var nrArguments = 14;
    var createSlider= true;
    if (arguments.length == nrArguments) {    
        this.id = id; //an internal id, this id is not used in the SVG Dom tree
        this.parentNode = parentNode; //the parentNode, string or nodeReference
        this.x1 = x1; //the start point x of the slider
        this.y1 = y1; //the start point y of the slider
        this.value1 = value1; //the value at the start point, min slider value
        this.x2 = x2; //the end point x of the slider 
        this.y2 = y2; //the end point y of the slider
        this.value2 = value2; //the value at the end point, max slider value
        this.startVal = startVal; //the initial value of the slider
        this.value = startVal; //the current value of the slider
        this.sliderStyles = sliderStyles; //the color of the slider line
        this.invisSliderWidth = invisSliderWidth; //the width of invisible part of the slider
        this.sliderSymb = sliderSymb; //the id for the movable symbol to be placed on the slider line 
        this.functionToCall = functionToCall; //the callback function
        this.mouseMoveBool = mouseMoveBool; //boolean value to indicate if the slider gives immediate feedback or not
        this.length = toPolarDist((this.x2 - this.x1),(this.y2 - this.y1));
        this.direction = toPolarDir((this.x2 - this.x1),(this.y2 - this.y1));
        this.invisSliderLine = null;
        this.slideStatus = 0;
    }
    else {
        createSlider = false;
        alert("Error in slider ("+id+") constructor: wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");
    }
    if (createSlider) {
        this.createSlider();
    }
    else {
        alert("Could not create slider with id '"+id+"' due to errors in the constructor parameters");        
    }
}

//create slider
slider.prototype.createSlider = function() {
    var result = this.testParent();
    if (result) {
        this.invisSliderLine = document.createElementNS(svgNS,"line");
        this.invisSliderLine.setAttributeNS(null,"x1",this.x1);
        this.invisSliderLine.setAttributeNS(null,"y1",this.y1);
        this.invisSliderLine.setAttributeNS(null,"x2",this.x2);
        this.invisSliderLine.setAttributeNS(null,"y2",this.y2);
        this.invisSliderLine.setAttributeNS(null,"stroke","black");
        this.invisSliderLine.setAttributeNS(null,"stroke-width",this.invisSliderWidth);
        //note that due to bugs in Opera9 and Firefox I can't use pointer-events here
        this.invisSliderLine.setAttributeNS(null,"stroke-opacity","0");
        this.invisSliderLine.setAttributeNS(null,"fill-opacity","0");
        this.invisSliderLine.setAttributeNS(null,"stroke-linecap","square");
        this.invisSliderLine.addEventListener("mousedown",this,false);
        this.parentGroup.appendChild(this.invisSliderLine);
        this.visSliderLine = document.createElementNS(svgNS,"line");
        this.visSliderLine.setAttributeNS(null,"x1",this.x1);
        this.visSliderLine.setAttributeNS(null,"y1",this.y1);
        this.visSliderLine.setAttributeNS(null,"x2",this.x2);
        this.visSliderLine.setAttributeNS(null,"y2",this.y2);
        for (var attrib in this.sliderStyles) {
            this.visSliderLine.setAttributeNS(null,attrib,this.sliderStyles[attrib]);
        }
        this.visSliderLine.setAttributeNS(null,"pointer-events","none");
        this.parentGroup.appendChild(this.visSliderLine);
        this.sliderSymbol = document.createElementNS(svgNS,"use");
        this.sliderSymbol.setAttributeNS(xlinkNS,"xlink:href","#"+this.sliderSymb);
        var myStartDistance = this.length - ((this.value2 - this.startVal) / (this.value2 - this.value1)) * this.length;
        var myPosX = this.x1 + toRectX(this.direction,myStartDistance);
        var myPosY = this.y1 + toRectY(this.direction,myStartDistance);
        var myTransformString = "translate("+myPosX+","+myPosY+") rotate(" + Math.round(this.direction / Math.PI * 180) + ")";
        this.sliderSymbol.setAttributeNS(null,"transform",myTransformString);
        this.sliderSymbol.setAttributeNS(null,"pointer-events","none");
        this.parentGroup.appendChild(this.sliderSymbol);
    }
    else {
        alert("could not create or reference 'parentNode' of slider with id '"+this.id+"'");            
    }
}

//test if parent group exists
slider.prototype.testParent = function() {
    //test if of type object
    var nodeValid = false;
    if (typeof(this.parentNode) == "object") {
        if (this.parentNode.nodeName == "svg" || this.parentNode.nodeName == "g" || this.parentNode.nodeName == "svg:svg" || this.parentNode.nodeName == "svg:g") {
            this.parentGroup = this.parentNode;
            nodeValid = true;
        }
    }
    else if (typeof(this.parentNode) == "string") { 
        //first test if button group exists
        if (!document.getElementById(this.parentNode)) {
            this.parentGroup = document.createElementNS(svgNS,"g");
            this.parentGroup.setAttributeNS(null,"id",this.parentNode);
            document.documentElement.appendChild(this.parentGroup);
            nodeValid = true;
           }
           else {
               this.parentGroup = document.getElementById(this.parentNode);
               nodeValid = true;
           }
       }
       return nodeValid;
}

//remove all slider elements
slider.prototype.removeSlider = function() {
    this.parentGroup.removeChild(this.sliderSymbol);
    this.parentGroup.removeChild(this.visSliderLine);
    this.parentGroup.removeChild(this.invisSliderLine);
}

//handle events
slider.prototype.handleEvent = function(evt) {
    this.drag(evt);
}

//drag slider
slider.prototype.drag = function(evt) {
    if (evt.type == "mousedown" || (evt.type == "mousemove" && this.slideStatus == 1)) {
        //get coordinate in slider coordinate system
        var coordPoint = myMapApp.calcCoord(evt,this.invisSliderLine);
        //draw normal line for first vertex
        var ax = this.x2 - this.x1;
        var ay = this.y2 - this.y1;
        //normal vector 1
        var px1 = parseFloat(this.x1) + ay * -1;
        var py1 = parseFloat(this.y1) + ax;
        //normal vector 2
        var px2 = parseFloat(this.x2) + ay * -1;
        var py2 = parseFloat(this.y2) + ax;
                
        if (leftOfTest(coordPoint.x,coordPoint.y,this.x1,this.y1,px1,py1) == 0 && leftOfTest(coordPoint.x,coordPoint.y,this.x2,this.y2,px2,py2) == 1) {
            if (evt.type == "mousedown" && (evt.detail == 1 || evt.detail == 0)) {
                this.slideStatus = 1;
                document.documentElement.addEventListener("mousemove",this,false);
                document.documentElement.addEventListener("mouseup",this,false);
            }
            myNewPos = intersect2lines(this.x1,this.y1,this.x2,this.y2,coordPoint.x,coordPoint.y,coordPoint.x + ay * -1,coordPoint.y + ax);
            var myPercentage = toPolarDist(myNewPos['x'] - this.x1,myNewPos['y'] - this.y1) / this.length;
            this.value = this.value1 + myPercentage * (this.value2 - this.value1);
        }
        else {
            var myNewPos = new Array();
            if (leftOfTest(coordPoint.x,coordPoint.y,this.x1,this.y1,px1,py1) == 0 && leftOfTest(coordPoint.x,coordPoint.y,this.x2,this.y2,px2,py2) == 0) {
                //more than max
                this.value = this.value2;
                myNewPos['x'] = this.x2;
                myNewPos['y'] = this.y2;
            }
            if (leftOfTest(coordPoint.x,coordPoint.y,this.x1,this.y1,px1,py1) == 1 && leftOfTest(coordPoint.x,coordPoint.y,this.x2,this.y2,px2,py2) == 1) {
                //less than min
                this.value = this.value1;
                myNewPos['x'] = this.x1;
                myNewPos['y'] = this.y1;
            }
        }
        var myTransformString = "translate("+myNewPos['x']+","+myNewPos['y']+") rotate(" + Math.round(this.direction / Math.PI * 180) + ")";
        this.sliderSymbol.setAttributeNS(null,"transform",myTransformString);
        this.fireFunction();
    }
    if (evt.type == "mouseup" && (evt.detail == 1 || evt.detail == 0)) {
        if (this.slideStatus == 1) {
            this.slideStatus = 2;
            document.documentElement.removeEventListener("mousemove",this,false);
            document.documentElement.removeEventListener("mouseup",this,false);
            this.fireFunction();
        }
        this.slideStatus = 0;
    }
}

//this code is executed, after the slider is released
//you can use switch/if to detect which slider was used (use this.id) for that
slider.prototype.fireFunction = function() {
    if (this.slideStatus == 1 && this.mouseMoveBool == true) {
        if (typeof(this.functionToCall) == "function") {
            this.functionToCall("change",this.id,this.value);
        }
        if (typeof(this.functionToCall) == "object") {
            this.functionToCall.getSliderVal("change",this.id,this.value);
        }
        if (typeof(this.functionToCall) == undefined) {
            return;
        }
    }
    if (this.slideStatus == 2) {
        if (typeof(this.functionToCall) == "function") {
            this.functionToCall("release",this.id,this.value);
        }
        if (typeof(this.functionToCall) == "object") {
            this.functionToCall.getSliderVal("release",this.id,this.value);
        }
        if (typeof(this.functionToCall) == undefined) {
            return;
        }
    }
}

slider.prototype.getValue = function() {
    return this.value;
}

//this is to set the value from other scripts
slider.prototype.setValue = function(value,fireFunction) {
    this.value = value;
    var myPercAlLine = (this.value - this.value1) / (this.value2 - this.value1);
    var myPosX = this.x1 + toRectX(this.direction,this.length * myPercAlLine);
    var myPosY = this.y1 + toRectY(this.direction,this.length * myPercAlLine);
    var myTransformString = "translate("+myPosX+","+myPosY+") rotate(" + Math.round(this.direction / Math.PI * 180) + ")";
    this.sliderSymbol.setAttributeNS(null,"transform",myTransformString);
    if (fireFunction) {
        //temporary set slideStatus
        this.slideStatus = 2;
        this.fireFunction();
        this.slideStatus = 0;
    }
}

slider.prototype.moveTo = function(x1,y1,x2,y2) {
    this.x1 = x1;
    this.y1 = y1;
    this.x2 = x2;
    this.y2 = y2;
    this.invisSliderLine.setAttributeNS(null,"x1",this.x1);
    this.invisSliderLine.setAttributeNS(null,"y1",this.y1);
    this.invisSliderLine.setAttributeNS(null,"x2",this.x2);
    this.invisSliderLine.setAttributeNS(null,"y2",this.y2);
    this.visSliderLine.setAttributeNS(null,"x1",this.x1);
    this.visSliderLine.setAttributeNS(null,"y1",this.y1);
    this.visSliderLine.setAttributeNS(null,"x2",this.x2);
    this.visSliderLine.setAttributeNS(null,"y2",this.y2);
    //reset direction and length of the slider
    this.length = toPolarDist((this.x2 - this.x1),(this.y2 - this.y1));
    this.direction = toPolarDir((this.x2 - this.x1),(this.y2 - this.y1));
    //reset the value to correctly reposition element
    this.setValue(this.getValue(),false);
}
