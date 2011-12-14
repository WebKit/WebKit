/*
Scripts to create interactive tabs in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 1.2, 2006-04-03
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:
* none so far

----

Documentation: http://www.carto.net/papers/svg/gui/tabgroup/

----

current version: 1.2.1

version history:
1.0 (2006-03-11)
initial version

1.1 (2006-04-03)
added ".moveTo(x,y)" and ".resize(width,height)" methods, added an additional g-element to hold transform values

1.2 (2006-06-19)
this.parentNode can now also be of type node reference (g or svg element); fixed a small bug when "hideContent" was set to true; changed this.parentGroup and introduced this.tabGroup
introduced id for group where content can be added, id name is: this.id+"__"+i+"_content"

1.2.1 (2006-07-07)
fixed a bug for multiline tabs (dy attribute of the tab texts). This bug was apparent when having more than two lines of text in the tabs

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

original document site: http://www.carto.net/papers/svg/gui/tabgroup/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/tabgroup/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

function tabgroup(id,parentNode,transx,transy,width,height,tabheight,cornerLeft,cornerRight,tabmargins,spaceBetweenTabs,tabStyles,activetabBGColor,tabwindowStyles,tabtextStyles,tabTitles,activeTabindex,hideContent,functionToCall) {
    var nrArguments = 19;
    var createTabgroup = true;
    if (arguments.length == nrArguments) {
        this.id = id;
        this.parentNode = parentNode; //can be of type string (id) or node reference (svg or g node)
        this.transx = transx;
        this.transy = transy;
        this.x = 0;
        this.y = 0;
        this.width = width;
        this.height = height;
        this.tabheight = tabheight;
        this.cornerLeft = cornerLeft; //values are "rect","round","triangle"
        this.cornerRight = cornerRight; //values are "rect","round","triangle"
        this.tabmargins = tabmargins;
        this.spaceBetweenTabs = spaceBetweenTabs;
        this.tabStyles = tabStyles;
        if (!this.tabStyles["fill"]) {
            this.tabStyles["fill"] = "lightgray";
        }
        this.activetabBGColor = activetabBGColor;
        this.tabwindowStyles = tabwindowStyles;
        this.tabtextStyles = tabtextStyles;
        if (!this.tabtextStyles["font-size"]) {
            this.tabtextStyles["font-size"] = 15;
        }
        this.tabTitles = tabTitles;
        if (this.tabTitles instanceof Array) {
            if (this.tabTitles.length == 0) {
                createTabgroup = false;
                alert("Error ("+this.id+"): the array 'tabTitles' has no elements!");
            }
        }
        else {
            createTabgroup = false;
            alert("Error ("+this.id+"): the array 'tabTitles' is not of type array!");
        }
        this.activeTabindex = activeTabindex;
        if (this.activeTabindex >= this.tabTitles.length) {
            createTabgroup = false;
            this.outOfBoundMessage(this.activeTabindex);
        }
        this.hideContent = hideContent; //boolean, defines whether the display of a tab should be set to "none","inherit"
        this.functionToCall = functionToCall;
        if (!(typeof(this.functionToCall) == "function" || typeof(this.functionToCall) == "object" || typeof(this.functionToCall) == "undefined")) {
            createTabgroup = false;
            alert("Error ("+this.id+"): functionToCall is of type '"+typeof(this.functionToCall)+"' and not of type 'function', 'object' or 'undefined'");
        }
    }
    else {
        createTabgroup = false;
        alert("Error ("+id+"): wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");
    }
    if (createTabgroup) {
        this.parentGroup = null; //later a node reference to the parent group
        this.tabGroup = null; //later a reference to the group within the parentGroup
        this.tabwindows = new Array();
        this.timer = new Timer(this); //a Timer instance for calling the functionToCall
        this.timerMs = 200; //a constant of this object that is used in conjunction with the timer - functionToCall is called after 200 ms
        this.createTabGroup();
    }
    else {
        alert("Could not create tabgroup with id '"+this.id+"' due to errors in the constructor parameters");
    }
}

tabgroup.prototype.createTabGroup = function() {
    var result = this.testParent();
    if (result) {
    this.tabGroup = document.createElementNS(svgNS,"g");
    this.tabGroup.setAttributeNS(null,"transform","translate("+this.transx+","+this.transy+")");
    this.parentGroup.appendChild(this.tabGroup);
    //loop to create all tabs
    var currentX = this.x;
    for (var i=0;i<this.tabTitles.length;i++) {
        currentLeft = currentX;
        this.tabwindows[i] = new Array();
        //create group
        this.tabwindows[i]["group"] = document.createElementNS(svgNS,"g");
        this.tabGroup.appendChild(this.tabwindows[i]["group"]);
        //create tabTitle
        var tabtitles = this.tabTitles[i].split("\n");
        this.tabwindows[i]["tabTitle"] = document.createElementNS(svgNS,"text");
        this.tabwindows[i]["tabTitle"].setAttributeNS(null,"y",this.y + this.tabtextStyles["font-size"]);
        var value="";
        for (var attrib in this.tabtextStyles) {
            value = this.tabtextStyles[attrib];
            if (attrib == "font-size") {
                value += "px";
            }
            this.tabwindows[i]["tabTitle"].setAttributeNS(null,attrib,value);
        }
        this.tabwindows[i]["tabTitle"].setAttributeNS(null,"pointer-events","none");
        //create tspans and add text contents
        for (var j=0;j<tabtitles.length;j++) {
            var tspan = document.createElementNS(svgNS,"tspan");
            tspan.setAttributeNS(null,"x",currentX+this.tabmargins);
            var dy = this.tabtextStyles["font-size"]*1.1;
            if (j == 0) {
                dy = 0;
            }
            tspan.setAttributeNS(null,"dy",dy);
            var textNode = document.createTextNode(tabtitles[j]);
            tspan.appendChild(textNode);
            this.tabwindows[i]["tabTitle"].appendChild(tspan);
        }
        this.tabwindows[i]["group"].appendChild(this.tabwindows[i]["tabTitle"]);
        //get bbox
        var bbox = this.tabwindows[i]["tabTitle"].getBBox();
        currentX = Math.round(currentLeft + this.tabmargins * 2 + bbox.width);
        //check if text-anchor is middle and shift the x-values accordingly
        if (this.tabtextStyles["text-anchor"]) {
            if (this.tabtextStyles["text-anchor"] == "middle") {
                this.tabwindows[i]["tabTitle"].setAttributeNS(null,"transform","translate("+(bbox.width * 0.5)+" 0)");
            }
        }
        //now draw tabwindow background
        this.tabwindows[i]["tabbg"] = document.createElementNS(svgNS,"path");
        for (var attrib in this.tabwindowStyles) {
            this.tabwindows[i]["tabbg"].setAttributeNS(null,attrib,this.tabwindowStyles[attrib]);
        }
        //start the path for tab windows
        var d = "M";
        //left corner of tab
        if (this.cornerLeft == "rect") {
            d += currentLeft+" "+this.y;
        }
        if (this.cornerLeft == "triangle") {
            d += currentLeft+" "+(this.y+this.tabmargins)+"L"+(currentLeft+this.tabmargins)+" "+this.y;
        }
        if (this.cornerLeft == "round") {
            d += currentLeft+" "+(this.y+this.tabmargins)+"a"+this.tabmargins+" "+this.tabmargins+" 0 0 1 "+this.tabmargins+" -"+this.tabmargins;
        }
        //right corner of tab
        if (this.cornerRight == "rect") {
            d += "L"+currentX+" "+this.y;
        }
        if (this.cornerRight == "triangle") {
            d += "L"+(currentX-this.tabmargins)+" "+this.y+"L"+currentX+" "+(this.y+this.tabmargins);
        }
        if (this.cornerRight == "round") {
            d += "L"+(currentX-this.tabmargins)+" "+this.y+"a"+this.tabmargins+" "+this.tabmargins+" 0 0 1 "+this.tabmargins+" "+this.tabmargins;
        }
        d += "L"+currentX+" "+(this.y+this.tabheight);
        //complete the path for tab
        var dtab = d + "L"+currentLeft+" "+(this.y+this.tabheight)+"z";
        //complete the path for tab window
        d += "L"+(this.x+this.width)+" "+(this.y+this.tabheight)+"L"+(this.x+this.width)+" "+(this.y+this.height);
        d += "L"+this.x+" "+(this.y+this.height);
        if (currentLeft == this.x) {
            d += "z";
        }
        else {
            d += "L"+this.x+" "+(this.y+this.tabheight)+"L"+currentLeft+" "+(this.y+this.tabheight)+"z";
        }
        this.tabwindows[i]["tabbg"].setAttributeNS(null,"d",d);
        this.tabwindows[i]["group"].insertBefore(this.tabwindows[i]["tabbg"],this.tabwindows[i]["tabTitle"]);
        //create tab element
        this.tabwindows[i]["tab"] = document.createElementNS(svgNS,"path");
        for (var attrib in this.tabStyles) {
            this.tabwindows[i]["tab"].setAttributeNS(null,attrib,this.tabStyles[attrib]);
        }
        this.tabwindows[i]["tab"].setAttributeNS(null,"d",dtab);
        this.tabwindows[i]["tab"].setAttributeNS(null,"id",this.id+"__"+i);
        this.tabwindows[i]["tab"].addEventListener("click",this,false);
        this.tabwindows[i]["group"].insertBefore(this.tabwindows[i]["tab"],this.tabwindows[i]["tabTitle"]);
        //create group for tab content
        this.tabwindows[i]["content"] = document.createElementNS(svgNS,"g");
        this.tabwindows[i]["content"].setAttributeNS(null,"id",this.id+"__"+i+"_content");
        if (this.hideContent) {
            this.tabwindows[i]["content"].setAttributeNS(null,"display","none");
        }
        this.tabwindows[i]["group"].appendChild(this.tabwindows[i]["content"]);
        //set tab activate status
        this.tabwindows[i].activeStatus = true;
        //increment currentX with the space between tabs
        currentX += this.spaceBetweenTabs;
    }
    //activate one tab
    this.tabwindows[this.activeTabindex]["tab"].setAttributeNS(null,"fill",this.activetabBGColor);
    this.tabGroup.appendChild(this.tabwindows[this.activeTabindex]["group"]);
    if (this.hideContent) {
        this.tabwindows[this.activeTabindex]["content"].setAttributeNS(null,"display","inherit");
    }
    }
    else {
        alert("could not create or reference 'parentNode' of tabgroup with id '"+this.id+"'");
    }
 }
 
 //test if window group exists or create a new group at the end of the file
tabgroup.prototype.testParent = function() {
    //test if of type object
    var nodeValid = false;
    if (typeof(this.parentNode) == "object") {
        if (this.parentNode.nodeName == "svg" || this.parentNode.nodeName == "g") {
            this.parentGroup = this.parentNode;
            nodeValid = true;
        }
    }
    else if (typeof(this.parentNode) == "string") { 
        //first test if Windows group exists
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

tabgroup.prototype.handleEvent = function(evt) {
    var tab = evt.target;
    var id = tab.getAttributeNS(null,"id");
    var idArray = id.split("__");
    var index = parseInt(idArray[1]);
    this.activateTabByIndex(index,false);
    //firefunction
    this.timer.setTimeout("fireFunction",this.timerMs);
}

tabgroup.prototype.fireFunction = function() {
    if (typeof(this.functionToCall) == "function") {
        this.functionToCall(this.id,this.tabTitles[this.activeTabindex],this.activeTabindex);
    }
    if (typeof(this.functionToCall) == "object") {
        this.functionToCall.tabActivated(this.id,this.tabTitles[this.activeTabindex],this.activeTabindex);
    }
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}

tabgroup.prototype.activateTabByIndex = function(tabindex,fireFunction) {
    if (tabindex >= this.tabTitles.length) {
        this.outOfBoundMessage(tabindex);
        tabindex = 0;
    }
    //set old active tab to inactive
    this.tabwindows[this.activeTabindex]["tab"].setAttributeNS(null,"fill",this.tabStyles["fill"]);
    if (this.hideContent) {
        this.tabwindows[this.activeTabindex]["content"].setAttributeNS(null,"display","none");
    }
    //set new index
    this.activeTabindex = tabindex;
    //activate new tab
    this.tabwindows[this.activeTabindex]["tab"].setAttributeNS(null,"fill",this.activetabBGColor);
    //reorder tabs
    this.tabGroup.appendChild(this.tabwindows[this.activeTabindex]["group"]);
    //set display
    if (this.hideContent) {
        this.tabwindows[this.activeTabindex]["content"].setAttributeNS(null,"display","inherit");
    }
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

tabgroup.prototype.activateTabByTitle = function(title,fireFunction) {
    var tabindex = -1;
    for (var i=0;i<this.tabTitles.length;i++) {
        if (this.tabTitles[i] == title) {
            tabindex = i;
            break;
        }
    }
    if (tabindex != -1) {
        this.activateTabByIndex(tabindex,fireFunction);
    }
    else {
        alert("Error ("+this.id+"): Could not find title '"+title+"' in array tabTitles!");
    }
}

//add content to this.tabwindows[tabindex]["content"]
tabgroup.prototype.addContent = function(node,tabindex,inheritDisplay) {
    if (tabindex >= this.tabTitles.length) {
        this.outOfBoundMessage(tabindex);
    }
    else {
        if (typeof(node) == "string") {
            node = document.getElementById(node);
        }
        if (node != undefined || node != "null") {
            if (inheritDisplay) {
                node.setAttributeNS(null,"display","inherit");
            }
            this.tabwindows[tabindex]["content"].appendChild(node);
        }
    }
}

//remove content from this.tabwindows[tabindex]["content"]
tabgroup.prototype.removeContent = function(node,tabindex) {
    var deletedNode = undefined;
    if (tabindex >= this.tabTitles.length) {
        this.outOfBoundMessage(tabindex);
    }
    else {
        if (typeof(node) == "string") {
            node = document.getElementById(node);
        }
        if (node != undefined || node != "null") {
            deletedNode = this.tabwindows[tabindex]["content"].removeChild(node);
        }
    }
    return deletedNode;
}

//move the tab, reference is upper left corner
tabgroup.prototype.moveTo = function(x,y) {
    this.transx = x;
    this.transy = y;
    this.tabGroup.setAttributeNS(null,"transform","translate("+this.transx+","+this.transy+")");
}

//resize tabgroup
tabgroup.prototype.resize = function(width,height) {
    this.width = width;
    this.height = height;
    //loop to change all tab sizes
    var currentX = this.x;
    for (var i=0;i<this.tabTitles.length;i++) {
        currentLeft = currentX;
        //get bbox
        var bbox = this.tabwindows[i]["tabTitle"].getBBox();
        currentX = Math.round(currentLeft + this.tabmargins * 2 + bbox.width);
        //start the path for tab windows
        var d = "M";
        //left corner of tab
        if (this.cornerLeft == "rect") {
            d += currentLeft+" "+this.y;
        }
        if (this.cornerLeft == "triangle") {
            d += currentLeft+" "+(this.y+this.tabmargins)+"L"+(currentLeft+this.tabmargins)+" "+this.y;
        }
        if (this.cornerLeft == "round") {
            d += currentLeft+" "+(this.y+this.tabmargins)+"a"+this.tabmargins+" "+this.tabmargins+" 0 0 1 "+this.tabmargins+" -"+this.tabmargins;
        }
        //right corner of tab
        if (this.cornerRight == "rect") {
            d += "L"+currentX+" "+this.y;
        }
        if (this.cornerRight == "triangle") {
            d += "L"+(currentX-this.tabmargins)+" "+this.y+"L"+currentX+" "+(this.y+this.tabmargins);
        }
        if (this.cornerRight == "round") {
            d += "L"+(currentX-this.tabmargins)+" "+this.y+"a"+this.tabmargins+" "+this.tabmargins+" 0 0 1 "+this.tabmargins+" "+this.tabmargins;
        }
        d += "L"+currentX+" "+(this.y+this.tabheight);
        //complete the path for tab window
        d += "L"+(this.x+this.width)+" "+(this.y+this.tabheight)+"L"+(this.x+this.width)+" "+(this.y+this.height);
        d += "L"+this.x+" "+(this.y+this.height);
        if (currentLeft == this.x) {
            d += "z";
        }
        else {
            d += "L"+this.x+" "+(this.y+this.tabheight)+"L"+currentLeft+" "+(this.y+this.tabheight)+"z";
        }
        //set modified path elements
        this.tabwindows[i]["tabbg"].setAttributeNS(null,"d",d);
        //increment currentX with the space between tabs
        currentX += this.spaceBetweenTabs;
    }
}

//deactivate a single tab
tabgroup.prototype.disableSingleTab = function(tabindex) {
    if (tabindex >= this.tabTitles.length) {
        this.outOfBoundMessage(tabindex);
    }
    else {
        this.tabwindows[tabindex]["activeStatus"] = false;
        if (!this.tabwindows[tabindex]["tabClone"]) {
            this.tabwindows[tabindex]["tabClone"] = this.tabwindows[tabindex]["tab"].cloneNode(false);
            this.tabwindows[tabindex]["tabClone"].removeEventListener("click",this,false);
            this.tabwindows[tabindex]["tabClone"].setAttributeNS(null,"fill-opacity",0.5);
            if (this.tabwindows[tabindex]["tabClone"].hasAttributeNS(null,"cursor")){
                this.tabwindows[tabindex]["tabClone"].removeAttributeNS(null,"cursor");
            }
            this.tabwindows[tabindex]["group"].appendChild(this.tabwindows[tabindex]["tabClone"]);
        }
        else {
            this.tabwindows[tabindex]["tabClone"].setAttributeNS(null,"display","inherit");
            this.tabwindows[tabindex]["group"].appendChild(this.tabwindows[tabindex]["tabClone"]);
        }
    }
}

//deactivate all tabs
tabgroup.prototype.disableAllTabs = function() {
    for (var i=0;i<this.tabTitles.length;i++) {
        this.disableSingleTab(i);
    }
}

//deactivate all tabs
tabgroup.prototype.enableAllTabs = function() {
    for (var i=0;i<this.tabTitles.length;i++) {
        this.enableSingleTab(i);
    }
}

//activate a single tab
tabgroup.prototype.enableSingleTab = function(tabindex) {
    if (tabindex >= this.tabTitles.length) {
        this.outOfBoundMessage(tabindex);
    }
    else {
        this.tabwindows[tabindex]["activeStatus"] = true;
        if (this.tabwindows[tabindex]["tabClone"]) {
            this.tabwindows[tabindex]["tabClone"].setAttributeNS(null,"display","none");
        }
    }
}

//out of bound error message
tabgroup.prototype.outOfBoundMessage = function(tabindex) {
    alert("Error ("+this.id+"): the 'tabindex' (value: "+tabindex+") is out of bounds!\nThe index nr is bigger than the number of tabs.");
}
