/*
Scripts to create interactive scrollbars in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 0.9.1, 2006-10-30
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:

----

current version: 0.9.1

version history:
0.9 (2006-10-12)
initial version

0.9.1 (2006-10-30)
added .show(), .hide() and .remove() methods

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

original document site: http://www.carto.net/papers/svg/gui/scrollbar/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/scrollbar/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

function scrollbar(id,parentNode,x,y,width,height,startValue,endValue,initialHeightPerc,initialOffset,scrollStep,scrollButtonLocations,scrollbarStyles,scrollerStyles,triangleStyles,highlightStyles,functionToCall) {
    var nrArguments = 17;
    var createScrollbar = true;
    if (arguments.length == nrArguments) {
        //get constructor variables
        this.id = id;
        this.parentNode = parentNode;
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
        this.startValue = startValue;
        this.endValue = endValue;
        this.initialHeightPerc = initialHeightPerc;
        this.initialOffset = initialOffset;
        this.scrollStep = scrollStep; //this value indicates how much the slider will scroll when the arrow buttons are pressed, values are in percentage
        this.scrollButtonLocations = scrollButtonLocations;
        if (this.scrollButtonLocations != "bottom_bottom" && this.scrollButtonLocations != "top_bottom" && this.scrollButtonLocations && "top_top" && this.scrollButtonLocations != "none_none") {
            createScrollbar = false;
            alert("Error: parameter 'scrollButtonLocations' can only be of the following values: 'bottom_bottom' || 'top_top' || 'top_bottom' || 'none_none'.");        
        }
        this.scrollbarStyles = scrollbarStyles;
        this.scrollerStyles = scrollerStyles;
        this.triangleStyles = triangleStyles;
        this.highlightStyles = highlightStyles;
        this.functionToCall = functionToCall;
        //additional properties to be used later
        this.horizOrVertical = "vertical"; //specifies wether scrollbar is horizontal or vertical
        this.cellHeight = this.width; //the height or width of the buttons on top or bottom of the scrollbar
        this.scrollStatus = false; //indicates whether scrolling is active
        this.buttonScrollActive = false; //indicates whether the scrollbutton is currently being pressed
        this.scrollbarScrollActive = false; //indicates whether the scrollbar is currently being pressed
    }
    else {
        createScrollbar = false;
        alert("Error ("+id+"): wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");
    }
    if (createScrollbar) {
        //create scrollbar
        var result = this.testParent();
        if (result) {
            //timer stuff
            this.timer = new Timer(this); //a Timer instance for calling the functionToCall
            this.timerMs = 200; //a constant of this object that is used in conjunction with the timer - functionToCall is called after 200 ms
            this.createScrollbar();
        }
        else {
            alert("could not create or reference 'parentNode' of scrollbar with id '"+this.id+"'");
        }            
    }
    else {
        alert("Could not create scrollbar with id '"+id+"' due to errors in the constructor parameters");    
    }
}

//test if parent group exists or create a new group at the end of the file
scrollbar.prototype.testParent = function() {
    //test if of type object
    var nodeValid = false;
    this.parentGroup = document.createElementNS(svgNS,"g");
    if (typeof(this.parentNode) == "object") {
        if (this.parentNode.nodeName == "svg" || this.parentNode.nodeName == "g" || this.parentNode.nodeName == "svg:svg" || this.parentNode.nodeName == "svg:g") {
            this.parentNode.appendChild(this.parentGroup);
            nodeValid = true;
        }
    }
    else if (typeof(this.parentNode) == "string") { 
        //first test if button group exists
        if (!document.getElementById(this.parentNode)) {
            this.parentGroup.setAttributeNS(null,"id",this.parentNode);
            document.documentElement.appendChild(this.parentGroup);
            nodeValid = true;
           }
           else {
               document.getElementById(this.parentNode).appendChild(this.parentGroup);
               nodeValid = true;
           }
       }
       return nodeValid;
}

//create scrollbar geometry
scrollbar.prototype.createScrollbar = function() {
    //first determine if vertical of horizontal
    if (this.width > this.height) {
        this.horizOrVertical = "horizontal";
        this.cellHeight = this.height;
    }
    this.triangleFourth = Math.round(this.cellHeight / 4); //this is used to construct the triangles for the buttons

    if (this.scrollButtonLocations != "none_none") {
        this.scrollUpperRect = document.createElementNS(svgNS,"rect");
        this.scrollLowerRect = document.createElementNS(svgNS,"rect");
        this.scrollUpperTriangle = document.createElementNS(svgNS,"path");
        this.scrollLowerTriangle = document.createElementNS(svgNS,"path");
    }
 
    if (this.horizOrVertical == "vertical")  {
        this.scrollUpperRectX = this.x;
        this.scrollLowerRectX = this.x;
        switch (this.scrollButtonLocations)  {
            case "top_top":
                this.scrollbarX = this.x;
                this.scrollbarY = this.y + (2 * this.cellHeight);
                this.scrollbarWidth = this.width;
                this.scrollbarHeight = this.height - (2 * this.cellHeight);
                 this.scrollUpperRectY = this.y;
                 this.scrollLowerRectY = this.y + this.cellHeight;
                break;
            case "bottom_bottom":
                this.scrollbarX = this.x;
                this.scrollbarY = this.y;
                this.scrollbarWidth = this.width;
                this.scrollbarHeight = this.height - (2 * this.cellHeight);
                 this.scrollUpperRectY = this.y + this.height - (2 * this.cellHeight);
                 this.scrollLowerRectY = this.y + this.height - this.cellHeight;
                break;
            case "top_bottom":
                this.scrollbarX = this.x;
                this.scrollbarY = this.y + this.cellHeight;
                this.scrollbarWidth = this.width;
                this.scrollbarHeight = this.height - (2 * this.cellHeight);
                 this.scrollUpperRectY = this.y;
                 this.scrollLowerRectY = this.y + this.height - this.cellHeight;
                break;
            default:
                this.scrollbarX = this.x;
                this.scrollbarY = this.y;
                this.scrollbarWidth = this.width;
                this.scrollbarHeight = this.height;
                break;                
        }
        var myUpperTriPath = "M"+(this.scrollUpperRectX + this.cellHeight * 0.5)+" "+(this.scrollUpperRectY + this.triangleFourth)+" L"+(this.scrollUpperRectX + 3 * this.triangleFourth)+" "+(this.scrollUpperRectY + 3 * this.triangleFourth)+" L"+(this.scrollUpperRectX + this.triangleFourth)+" "+(this.scrollUpperRectY + 3 * this.triangleFourth)+" Z";
        var myLowerTriPath = "M"+(this.scrollLowerRectX + this.cellHeight * 0.5)+" "+(this.scrollLowerRectY + 3 * this.triangleFourth)+" L"+(this.scrollLowerRectX + this.triangleFourth)+" "+(this.scrollLowerRectY + this.triangleFourth)+" L"+(this.scrollLowerRectX + this.triangleFourth * 3)+" "+(this.scrollLowerRectY + this.triangleFourth)+" Z";
    }

    if (this.horizOrVertical == "horizontal")  {
        this.scrollUpperRectY = this.y;
        this.scrollLowerRectY = this.y;
        switch (this.scrollButtonLocations)  {
            case "top_top":
                this.scrollbarX = this.x + (2 * this.cellHeight);
                this.scrollbarY = this.y;
                this.scrollbarWidth = this.width - (2 * this.cellHeight);
                this.scrollbarHeight = this.height;
                this.scrollUpperRectX = this.x;
                 this.scrollLowerRectX = this.x + this.cellHeight;
                break;
            case "bottom_bottom":
                this.scrollbarX = this.x;
                this.scrollbarY = this.y;
                this.scrollbarWidth = this.width - (2 * this.cellHeight);
                this.scrollbarHeight = this.height;
                 this.scrollUpperRectX = this.x + this.width - (2 * this.cellHeight);
                 this.scrollLowerRectX = this.x + this.width - this.cellHeight;
                break;
            case "top_bottom":
                this.scrollbarX = this.x + this.cellHeight;
                this.scrollbarY = this.y;
                this.scrollbarWidth = this.width - (2 * this.cellHeight);
                this.scrollbarHeight = this.height;
                 this.scrollUpperRectX = this.x;
                 this.scrollLowerRectX = this.x + this.width - this.cellHeight;
                break;
            default:
                this.scrollbarX = this.x;
                this.scrollbarY = this.y;
                this.scrollbarWidth = this.width;
                this.scrollbarHeight = this.height;
                break;                
        }
        var myUpperTriPath = "M"+(this.scrollUpperRectX + this.triangleFourth)+" "+(this.scrollUpperRectY + this.triangleFourth * 2)+" L"+(this.scrollUpperRectX + 3 * this.triangleFourth)+" "+(this.scrollUpperRectY + this.triangleFourth)+" L"+(this.scrollUpperRectX + 3 * this.triangleFourth)+" "+(this.scrollUpperRectY + 3 * this.triangleFourth)+" Z";
        var myLowerTriPath = "M"+(this.scrollLowerRectX + this.triangleFourth * 3)+" "+(this.scrollLowerRectY + 2 * this.triangleFourth)+" L"+(this.scrollLowerRectX + this.triangleFourth)+" "+(this.scrollLowerRectY + this.triangleFourth * 3)+" L"+(this.scrollLowerRectX + this.triangleFourth)+" "+(this.scrollLowerRectY + this.triangleFourth)+" Z";
    }

    this.scrollbar = document.createElementNS(svgNS,"rect");
    this.scrollbar.setAttributeNS(null,"x",this.scrollbarX);
    this.scrollbar.setAttributeNS(null,"y",this.scrollbarY);
    this.scrollbar.setAttributeNS(null,"width",this.scrollbarWidth);
    this.scrollbar.setAttributeNS(null,"height",this.scrollbarHeight);
    this.scrollbar.setAttributeNS(null,"id","scrollbar_"+this.id);
    for (var attrib in this.scrollbarStyles) {
        this.scrollbar.setAttributeNS(null,attrib,this.scrollbarStyles[attrib]);
    }
    this.scrollbar.addEventListener("mousedown",this,false);
    this.parentGroup.appendChild(this.scrollbar);
    //now create scroller
    this.scroller = document.createElementNS(svgNS,"rect");
    if (this.horizOrVertical == "vertical")  {
        this.scroller.setAttributeNS(null,"x",this.scrollbarX);
        this.scrollerY = this.scrollbarY + (this.scrollbarHeight - this.scrollbarHeight * this.initialHeightPerc)  * this.initialOffset;
        this.scroller.setAttributeNS(null,"y",this.scrollerY);
        this.scroller.setAttributeNS(null,"width",this.scrollbarWidth);
        this.scrollerHeight = this.scrollbarHeight * this.initialHeightPerc;
        this.scroller.setAttributeNS(null,"height",this.scrollerHeight);
    }
    if (this.horizOrVertical == "horizontal")  {
        this.scrollerX = this.scrollbarX + (this.scrollbarWidth - this.scrollbarWidth * this.initialHeightPerc)  * this.initialOffset;
        this.scroller.setAttributeNS(null,"x",this.scrollerX);
        this.scroller.setAttributeNS(null,"y",this.scrollbarY);
        this.scrollerWidth = this.scrollbarWidth * this.initialHeightPerc;
        this.scroller.setAttributeNS(null,"width",this.scrollerWidth);
        this.scroller.setAttributeNS(null,"height",this.scrollbarHeight);
    }
    for (var attrib in this.scrollerStyles) {
        this.scroller.setAttributeNS(null,attrib,this.scrollerStyles[attrib]);
    }
    //need to add events here
    this.scroller.setAttributeNS(null,"id","scroller_"+this.id);
    this.scroller.addEventListener("mousedown",this,false);
    this.parentGroup.appendChild(this.scroller);
    //append rects and triangles
    if (this.scrollButtonLocations != "none_none") {
        //upper rect
        for (var attrib in this.scrollerStyles) {
            this.scrollUpperRect.setAttributeNS(null,attrib,this.scrollerStyles[attrib]);
        }
        this.scrollUpperRect.setAttributeNS(null,"x",this.scrollUpperRectX);
        this.scrollUpperRect.setAttributeNS(null,"y",this.scrollUpperRectY);
        this.scrollUpperRect.setAttributeNS(null,"width",this.cellHeight);
        this.scrollUpperRect.setAttributeNS(null,"height",this.cellHeight);
        this.scrollUpperRect.addEventListener("mousedown", this, false);
        this.scrollUpperRect.setAttributeNS(null,"id","scrollUpperRect_"+this.id);
        this.parentGroup.appendChild(this.scrollUpperRect);
        //lower rect
        for (var attrib in this.scrollerStyles) {
            this.scrollLowerRect.setAttributeNS(null,attrib,this.scrollerStyles[attrib]);
        }
        this.scrollLowerRect.setAttributeNS(null,"x",this.scrollLowerRectX);
        this.scrollLowerRect.setAttributeNS(null,"y",this.scrollLowerRectY);
        this.scrollLowerRect.setAttributeNS(null,"width",this.cellHeight);
        this.scrollLowerRect.setAttributeNS(null,"height",this.cellHeight);
        this.scrollLowerRect.addEventListener("mousedown", this, false);
        this.scrollLowerRect.setAttributeNS(null,"id","scrollLowerRect_"+this.id);
        this.parentGroup.appendChild(this.scrollLowerRect);
        //upper triangle
        this.scrollUpperTriangle.setAttributeNS(null,"d",myUpperTriPath);
        this.scrollUpperTriangle.setAttributeNS(null,"pointer-events","none");
        for (var attrib in this.triangleStyles) {
            this.scrollUpperTriangle.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
        }
        this.parentGroup.appendChild(this.scrollUpperTriangle);
        //lower triangle
        this.scrollLowerTriangle.setAttributeNS(null,"d",myLowerTriPath);
        this.scrollLowerTriangle.setAttributeNS(null,"pointer-events","none");
        for (var attrib in this.triangleStyles) {
            this.scrollLowerTriangle.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
        }
        this.parentGroup.appendChild(this.scrollLowerTriangle);
    }
 }
 
 scrollbar.prototype.handleEvent = function(evt) {
    var el = evt.target;
    var callerId = el.getAttributeNS(null,"id");
    if (evt.type == "mousedown") {
        if (callerId == "scroller_"+this.id) {
            //case the mouse went down on scroller
            this.scroll(evt);
        }
        if (callerId == "scrollLowerRect_"+this.id || callerId == "scrollUpperRect_"+this.id) {
            //this part is for scrolling per button
            this.buttonScrollActive = true;
            this.scrollDir = "down";
            this.currentScrollButton = evt.target;
            this.currentScrollTriangle = this.scrollLowerTriangle;
            if (callerId == "scrollUpperRect_"+this.id) {
                this.scrollDir = "up";
                this.currentScrollTriangle = this.scrollUpperTriangle;
            }
            document.documentElement.addEventListener("mouseup",this,false);
            //change appearance of button
            for (var attrib in this.scrollerStyles) {
                this.currentScrollButton.removeAttributeNS(null,attrib);
            }
            for (var attrib in this.highlightStyles) {
                this.currentScrollButton.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
            }
            //change appearance of triangle
            for (var attrib in this.triangleStyles) {
                this.currentScrollTriangle.removeAttributeNS(null,attrib);
            }
            for (var attrib in this.scrollerStyles) {
                this.currentScrollTriangle.setAttributeNS(null,attrib,this.scrollerStyles[attrib]);
            }
            this.scrollPercent(this.scrollDir,this.scrollStep);
            this.timer.setTimeout("scrollPerButton",400);
        }
        if (callerId == "scrollbar_"+this.id) {
            //this part is for scrolling when mouse is down on scrollbar
            var coords = myMapApp.calcCoord(evt,this.scrollbar);
            this.scrollbarScrollActive = true;
            this.scrollDir = "down";
            if (this.horizOrVertical == "vertical") {
                if (coords.y < this.scrollerY) {
                    this.scrollDir = "up";
                }
            }
            if (this.horizOrVertical == "horizontal") {
                if (coords.x < this.scrollerX) {
                    this.scrollDir = "up";
                }
            }
            document.documentElement.addEventListener("mouseup",this,false);
            //change styling
            for (var attrib in this.scrollerStyles) {
                this.scroller.removeAttributeNS(null,attrib);
            }
            for (var attrib in this.highlightStyles) {
                this.scroller.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
            }
            this.scrollPercent(this.scrollDir,this.scrollStep*10);
            this.timer.setTimeout("scrollPerScrollbar",400);
        }
    }
    if (evt.type == "mousemove" && this.scrollStatus) {
        //this is for scrolling per scroller
        this.scroll(evt);
    }
    if (evt.type == "mouseup") {
        //this is for finishing the different scroll modi
        if (this.scrollStatus) {
            //finishing scroll per scroller
            this.scroll(evt);
        }
        if (this.buttonScrollActive) {
            //finishing scroll per button
            this.buttonScrollActive = false;
            document.documentElement.removeEventListener("mouseup",this,false);
            //change appearance of button
            for (var attrib in this.highlightStyles) {
                this.currentScrollButton.removeAttributeNS(null,attrib);
            }
            for (var attrib in this.scrollerStyles) {
                this.currentScrollButton.setAttributeNS(null,attrib,this.scrollerStyles[attrib]);
            }
            //change appearance of triangle
            for (var attrib in this.scrollerStyles) {
                this.currentScrollTriangle.removeAttributeNS(null,attrib);
            }
            for (var attrib in this.triangleStyles) {
                this.currentScrollTriangle.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
            }
            this.currentScrollButton = undefined;
            this.currentScrollTriangle = undefined;
        }
        if (this.scrollbarScrollActive) {
            //finishing scroll per scrollbar
            this.scrollbarScrollActive = false;
            document.documentElement.removeEventListener("mouseup",this,false);        
            //change styling
            for (var attrib in this.highlightStyles) {
                this.scroller.removeAttributeNS(null,attrib);
            }
            for (var attrib in this.scrollerStyles) {
                this.scroller.setAttributeNS(null,attrib,this.scrollerStyles[attrib]);
            }
        }
    }
}

scrollbar.prototype.scroll = function(evt) {
    var coords = myMapApp.calcCoord(evt,this.scrollbar);
    if (evt.type == "mousedown") {
        document.documentElement.addEventListener("mousemove",this,false);
        document.documentElement.addEventListener("mouseup",this,false);
        this.scrollStatus = true;
        this.panCoords = coords;
        this.fireFunction("scrollStart");
        //change styling
        for (var attrib in this.scrollerStyles) {
            this.scroller.removeAttributeNS(null,attrib);
        }
        for (var attrib in this.highlightStyles) {
            this.scroller.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
        }
    }
    if (evt.type == "mousemove") {
        var diffX = coords.x - this.panCoords.x;
        var diffY = coords.y - this.panCoords.y;
        var scrollerDiff = false;
        if (this.horizOrVertical == "vertical") {
            var scrollerOrigY = this.scrollerY;
            this.scrollerY += diffY;
            if (this.scrollerY < this.scrollbarY) {
                this.scrollerY = this.scrollbarY;
            }
            if ((this.scrollerY + this.scrollerHeight) > (this.scrollbarY + this.scrollbarHeight)) {
                this.scrollerY = this.scrollbarY + this.scrollbarHeight - this.scrollerHeight;
            }
            this.scroller.setAttributeNS(null,"y",this.scrollerY);
            if (scrollerOrigY != this.scrollerY) {
                scrollerDiff = true;
            }
        }
        if (this.horizOrVertical == "horizontal") {
            var scrollerOrigX = this.scrollerX;
            this.scrollerX += diffX;
            if (this.scrollerX < this.scrollbarX) {
                this.scrollerX = this.scrollbarX;
            }
            if ((this.scrollerX + this.scrollerWidth) > (this.scrollbarX + this.scrollbarWidth)) {
                this.scrollerX = this.scrollbarX + this.scrollbarWidth - this.scrollerWidth;
            }
            this.scroller.setAttributeNS(null,"x",this.scrollerX);
            if (scrollerOrigX != this.scrollerX) {
                scrollerDiff = true;
            }
        }
        if (scrollerDiff) {
            this.fireFunction("scrollChange");
        }
        this.panCoords = coords;
    }
    if (evt.type == "mouseup") {
        this.scrollStatus = false;
        document.documentElement.removeEventListener("mousemove",this,false);
        document.documentElement.removeEventListener("mouseup",this,false);
        //change styling
        for (var attrib in this.highlightStyles) {
            this.scroller.removeAttributeNS(null,attrib);
        }
        for (var attrib in this.scrollerStyles) {
            this.scroller.setAttributeNS(null,attrib,this.scrollerStyles[attrib]);
        }
        this.fireFunction("scrollEnd");
    }
}

scrollbar.prototype.scrollPercent = function(direction,increment) {
    var currentValues = this.getValue();
    if (direction == "up") {
        increment = increment * -1;
    }
    var newPercent = currentValues.perc + increment;
    if (newPercent < 0) {
        newPercent = 0;
    }
    if (newPercent > 1) {
        newPercent = 1;
    }
    this.scrollToPercent(newPercent);
}

scrollbar.prototype.scrollToPercent = function(percValue) {
    if (percValue >= 0 && percValue <= 1) {
        if (this.horizOrVertical == "vertical") {
            var newY = this.scrollbarY + (this.scrollbarHeight - this.scrollerHeight) * percValue;
            this.scrollerY = newY;
            this.scroller.setAttributeNS(null,"y",newY);
        }
        if (this.horizOrVertical == "horizontal") {
            var newX = this.scrollbarX + (this.scrollbarWidth - this.scrollerWidth) * percValue;
            this.scrollerX = newX;
            this.scroller.setAttributeNS(null,"x",newX);
        }
        this.fireFunction("scrolledStep");
    }
    else {
        alert("error in method '.scrollToPercent()' of scrollbar with id '"+this.id+"'. Value out of range. Value needs to be in range 0 <= value <= 1.");
    }
}

scrollbar.prototype.scrollPerButton = function() {
    if (this.buttonScrollActive) {
            this.scrollPercent(this.scrollDir,this.scrollStep);
            this.timer.setTimeout("scrollPerButton",150);
    }
}

scrollbar.prototype.scrollPerScrollbar = function() {
    if (this.scrollbarScrollActive) {
            this.scrollPercent(this.scrollDir,this.scrollStep*5);
            this.timer.setTimeout("scrollPerScrollbar",150);
    }
}

scrollbar.prototype.scrollToValue = function(value) {
    if ((value >= this.startValue && value <= this.endValue) || (value <= this.startValue && value >= this.endValue)) {
        var percValue = value/(this.endValue - this.startValue);
        this.scrollToPercent(percValue);
    }
    else {
        alert("Error in scrollbar with id '"+this.id+"': the provided value '"+value+"' is out of range. The valid range is between '"+this.startValue+"' and '"+this.endValue+"'");
    }
}

scrollbar.prototype.getValue = function() {
    var perc;
    if (this.horizOrVertical == "vertical") {
        perc = (this.scrollerY - this.scrollbarY) / (this.scrollbarHeight - this.scrollerHeight);
    }
    if (this.horizOrVertical == "horizontal") {
        perc = (this.scrollerX - this.scrollbarX) / (this.scrollbarWidth - this.scrollerWidth);
    }
    var abs = this.startValue + (this.endValue - this.startValue) * perc;
    return {"abs":abs,"perc":perc};
}

scrollbar.prototype.fireFunction = function(changeType) {
    var values = this.getValue();
    if (typeof(this.functionToCall) == "function") {
        this.functionToCall(this.id,changeType,values.abs,values.perc);
    }
    if (typeof(this.functionToCall) == "object") {
        this.functionToCall.scrollbarChanged(this.id,changeType,values.abs,values.perc);
    }
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}

scrollbar.prototype.hide = function() {
    this.parentGroup.setAttributeNS(null,"display","none");
}

scrollbar.prototype.show = function() {
    this.parentGroup.setAttributeNS(null,"display","inherit");
}

scrollbar.prototype.remove = function() {
    this.parentGroup.parentNode.removeChild(this.parentGroup);
}
