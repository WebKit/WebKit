/*
Scripts to create interactive Windows in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 1.2, 2006-08-22
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:
* none so far

----

Documentation: http://www.carto.net/papers/svg/gui/Window/

----

current version: 1.2

version history:
1.0 (2006-02-09)
initial version

1.0.1 (2006-03-11)
changed parameters of constructor (styling system): now an array of literals containing presentation attributes. This allows for more flexibility in Styling. Added check for number of arguments.

1.1 (2006-03-13)
added additional parameter for .appendContent() and .insertContent(), added a .resize(method)

1.1.1 (2006-06-15)
added parameter fireFunction (of type boolean) to all methods that change the window state in order to allow script controlled changes without fireing the callback function

1.1.2 (2006-06-16)
renamed this.parentId to this.parentNode; this.parentNode can now also be of type node reference (g or svg element)

1.2 (2006-08-22)
added additional event types: "moveStart", "moveEnd" and "created", added method ".addWindowDecoration()", improved documentation.

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

original document site: http://www.carto.net/papers/svg/gui/Window/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/Window/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

function Window(id,parentNode,width,height,transX,transY,moveable,constrXmin,constrYmin,constrXmax,constrYmax,showContent,placeholderStyles,windowStyles,margin,titleBarVisible,statusBarVisible,titleText,statusText,closeButton,minimizeButton,maximizeButton,titlebarStyles,titlebarHeight,statusbarStyles,statusbarHeight,titletextStyles,statustextStyles,buttonStyles,functionToCall) {
    var nrArguments = 30;
    var createWindow= true;
    if (arguments.length == nrArguments) {
        this.id = id;
        this.parentNode = parentNode; //can be of type string (id) or node reference (svg or g node)
        this.width = width;
        this.height = height;
        this.transX = transX;
        this.transY = transY;
        this.constrXmin = constrXmin;
        this.constrYmin = constrYmin;
        this.constrXmax = constrXmax;
        this.constrYmax = constrYmax;
        this.showContent = showContent;
        this.placeholderStyles = placeholderStyles;
        this.moveable = moveable;
        this.windowStyles = windowStyles;
        this.margin = margin;
        this.titleBarVisible = titleBarVisible;
        this.statusBarVisible = statusBarVisible;
        this.titleText = titleText;
        this.statusText = statusText;
        this.closeButton = closeButton;
        this.minimizeButton = minimizeButton;
        this.maximizeButton = maximizeButton;
        this.titlebarStyles = titlebarStyles;
        this.titlebarHeight = titlebarHeight;
        this.statusbarStyles = statusbarStyles;
        this.statusbarHeight = statusbarHeight;
        this.titletextStyles = titletextStyles;
        if (!this.titletextStyles["font-size"]) {
            this.titletextStyles["font-size"] = 12;
        }
        this.statustextStyles = statustextStyles;
        if (!this.statustextStyles["font-size"]) {
            this.statustextStyles["font-size"] = 12;
        }
        this.buttonStyles = buttonStyles;
        if (!this.buttonStyles["fill"]) {
            this.buttonStyles["fill"] = "gainsboro";
        }
        if (!this.buttonStyles["stroke"]) {
            this.buttonStyles["stroke"] = "dimgray";
        }
        if (!this.buttonStyles["stroke-width"]) {
            this.buttonStyles["stroke-width"] = 1;
        }
        this.functionToCall = functionToCall;
        //now status and reference variables
        this.windowGroup = null; //later a reference to the window group
        this.parentGroup = null; //later a reference to the parent group of the window
        this.windowTitlebarGroup = null; //later a reference to the group containing the title bar elements of the group
        this.titleBar = null; //later a reference to the titleBar rectangle
        this.windowMainGroup = null; //later a reference to the group containing the window background and the content
        this.windowContentGroup = null; //later a reference to the content of the window
        this.shadowRect = null; //later a reference to a shadow rectangle
        this.shadowTitleRect = null; //later a reference to a shadow rectangle of the titlebar
        this.backgroundRect = null; //later a reference to the window background rectangle
        this.closeButtonInstance = null; //later a reference to the close button
        this.maximizeButtonInstance = null; //later a reference to the maximize button
        this.minimizeButtonInstance = null; //later a reference to the minimize button
        this.statusbar = null; //later a reference to the statusbar
        this.statusTextElement = null; //later a reference to the status text element
        this.statusTextNode = null; //later a reference to the text child node of the statusbar
        this.titleTextNode = null; //later a reference to the text child node of the titlebar
        this.panStatus = 0; //0 means not active, 1 means mousedown and initialized 2 means currently panning
        this.minimized = false; //status to indicate if window is minimized
        this.closed = false; //status to indicate if window is closed
        this.removed = false; //status to indicate if window is removed/available
        this.decorationGroup = null; //later a potential reference to the group containing a window decoration
        this.decorationGroupMinimized = null; //later a potential reference to the group containing the window decoration geometry that is set to display="none" when minimized
    }
    else {
        createWindow = false;
        alert("Error ("+id+"): wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");
    }
    if (createWindow) {
        this.timer = new Timer(this); //a Timer instance for calling the functionToCall
        this.timerMs = 200; //a constant of this object that is used in conjunction with the timer - functionToCall is called after 200 ms
        this.createWindow();
    }
    else {
        alert("Could not create Window with id '"+id+"' due to errors in the constructor parameters");        
    }
}

//create a new window
Window.prototype.createWindow = function() {
    var result = this.testParent();
    if (result) {
    //main group of the window
    this.windowGroup = document.createElementNS(svgNS,"g");
    this.windowGroup.setAttributeNS(null,"id",this.id);
    this.windowGroup.setAttributeNS(null,"transform","translate("+this.transX+","+this.transY+")");
    //create shadowRect to represent window if showContent variable is false
    this.shadowRect = document.createElementNS(svgNS,"rect");
    this.shadowRect.setAttributeNS(null,"width",this.width);
    this.shadowRect.setAttributeNS(null,"height",this.height);
    for (var attrib in this.placeholderStyles) {
        this.shadowRect.setAttributeNS(null,attrib,this.placeholderStyles[attrib]);
    }
    this.shadowRect.setAttributeNS(null,"display","none");
    this.windowGroup.appendChild(this.shadowRect);
    //create shadowRect to represent window if showContent variable is false
    this.shadowTitleRect = document.createElementNS(svgNS,"rect");
    this.shadowTitleRect.setAttributeNS(null,"width",this.width);
    this.shadowTitleRect.setAttributeNS(null,"height",this.titlebarHeight);
    for (var attrib in this.placeholderStyles) {
        this.shadowTitleRect.setAttributeNS(null,attrib,this.placeholderStyles[attrib]);
    }
    this.shadowTitleRect.setAttributeNS(null,"display","none");
    this.windowGroup.appendChild(this.shadowTitleRect);
    //group of the window titlebar
    this.windowTitlebarGroup = document.createElementNS(svgNS,"g");
    this.windowTitlebarGroup.setAttributeNS(null,"id","windowTitlebarGroup"+this.id);
    //group containing background and content
    this.windowMainGroup = document.createElementNS(svgNS,"g");
    this.windowMainGroup.setAttributeNS(null,"id","windowMainGroup"+this.id);
    //group later containing window content
    this.windowContentGroup = document.createElementNS(svgNS,"g");
    //create backgroundRect
    this.backgroundRect = document.createElementNS(svgNS,"rect");
    this.backgroundRect.setAttributeNS(null,"width",this.width);
    this.backgroundRect.setAttributeNS(null,"height",this.height);
    for (var attrib in this.windowStyles) {
        this.backgroundRect.setAttributeNS(null,attrib,this.windowStyles[attrib]);
    }    
    this.windowMainGroup.appendChild(this.backgroundRect);
    this.windowMainGroup.appendChild(this.windowContentGroup);
    this.windowGroup.appendChild(this.windowMainGroup);
    //create titlebar
    if (this.titleBarVisible) {
        this.titlebar = document.createElementNS(svgNS,"rect");
        this.titlebar.setAttributeNS(null,"width",this.width);
        this.titlebar.setAttributeNS(null,"height",this.titlebarHeight);
        for (var attrib in this.titlebarStyles) {
            this.titlebar.setAttributeNS(null,attrib,this.titlebarStyles[attrib]);
        }
        this.titlebar.setAttributeNS(null,"id","titleBar"+this.id);
        this.titlebar.setAttributeNS(null,"cursor","pointer");
        this.titlebar.addEventListener("click",this,false);
        this.titlebar.addEventListener("mousedown",this,false);
        this.windowTitlebarGroup.appendChild(this.titlebar);
        var titletext = document.createElementNS(svgNS,"text");
        titletext.setAttributeNS(null,"x",this.margin);
        titletext.setAttributeNS(null,"y",this.titlebarHeight - (this.titlebarHeight - this.titletextStyles["font-size"]));
        var value = "";
        for (var attrib in this.titletextStyles) {
            value = this.titletextStyles[attrib];
            if (attrib == "font-size") {
                value += "px";
            }
            titletext.setAttributeNS(null,attrib,value);
        }
        titletext.setAttributeNS(null,"pointer-events","none");
        if (this.titleText.length > 0) {
            this.titleTextNode = document.createTextNode(this.titleText);
        }
        else {
            this.titleTextNode = document.createTextNode(" ");
        }
        titletext.appendChild(this.titleTextNode);
        this.windowTitlebarGroup.appendChild(titletext);
    }
    //test if defs section exists or create a new one
    var defsSection = document.getElementsByTagName("defs").item(0);
    if (!defsSection) {
        defsSection = document.createElementNS(svgNS,"defs");
        document.documentElement.appendChild(defsSection);
    }
    //now create buttons
    var buttonPosition = this.width - this.margin - this.titlebarHeight * 0.5;
    var buttonWidth = this.titlebarHeight - this.margin * 2;
    //create close button
    if (this.closeButton) {
        //test if id closeButton exists in defs section or create a new closeButton symbol
        var closeButtonSymbol = document.getElementById("closeButton");
        if (!closeButtonSymbol) {
            var closeButtonSymbol = document.createElementNS(svgNS,"symbol");
            closeButtonSymbol.setAttributeNS(null,"id","closeButton");
            closeButtonSymbol.setAttributeNS(null,"overflow","visible");
            //create background rect
            var buttonRect = document.createElementNS(svgNS,"rect");
            buttonRect.setAttributeNS(null,"x",(buttonWidth / 2 * -1));
            buttonRect.setAttributeNS(null,"y",(buttonWidth / 2 * -1));
            buttonRect.setAttributeNS(null,"width",buttonWidth);
            buttonRect.setAttributeNS(null,"height",buttonWidth);
            buttonRect.setAttributeNS(null,"fill",this.buttonStyles["fill"]);
            buttonRect.setAttributeNS(null,"pointer-events","fill");
            closeButtonSymbol.appendChild(buttonRect);
            var buttonLine = document.createElementNS(svgNS,"line");
            buttonLine.setAttributeNS(null,"x1",(buttonWidth / 2 * -1));
            buttonLine.setAttributeNS(null,"x2",(buttonWidth / 2));
            buttonLine.setAttributeNS(null,"y1",(buttonWidth / 2 * -1));
            buttonLine.setAttributeNS(null,"y2",(buttonWidth / 2));
            buttonLine.setAttributeNS(null,"stroke",this.buttonStyles["stroke"]);
            buttonLine.setAttributeNS(null,"stroke-width",this.buttonStyles["stroke-width"]);
            buttonLine.setAttributeNS(null,"pointer-events","none");
            closeButtonSymbol.appendChild(buttonLine);
            var buttonLine = document.createElementNS(svgNS,"line");
            buttonLine.setAttributeNS(null,"x1",(buttonWidth / 2));
            buttonLine.setAttributeNS(null,"x2",(buttonWidth / 2 * -1));
            buttonLine.setAttributeNS(null,"y1",(buttonWidth / 2 * -1));
            buttonLine.setAttributeNS(null,"y2",(buttonWidth / 2));
            buttonLine.setAttributeNS(null,"stroke",this.buttonStyles["stroke"]);
            buttonLine.setAttributeNS(null,"stroke-width",this.buttonStyles["stroke-width"]);
            buttonLine.setAttributeNS(null,"pointer-events","none");
            closeButtonSymbol.appendChild(buttonLine);
            defsSection.appendChild(closeButtonSymbol);
        }
        this.closeButtonInstance = document.createElementNS(svgNS,"use");
        this.closeButtonInstance.setAttributeNS(null,"x",buttonPosition);
        this.closeButtonInstance.setAttributeNS(null,"y",this.titlebarHeight * 0.5);
        this.closeButtonInstance.setAttributeNS(null,"cursor","pointer");
        this.closeButtonInstance.setAttributeNS(null,"id","closeButton"+this.id);
        this.closeButtonInstance.setAttributeNS(xlinkNS,"href","#closeButton");
        this.closeButtonInstance.addEventListener("click",this,false);
        this.windowTitlebarGroup.appendChild(this.closeButtonInstance);
        buttonPosition -=  this.titlebarHeight;
    }
    
   //create maximize button
   if (this.maximizeButton) {
       //test if id maximizeButton exists in defs section or create a new maximizeButton symbol
       var maximizeButtonSymbol = document.getElementById("maximizeButton");
       if (!maximizeButtonSymbol) {
           var maximizeButtonSymbol = document.createElementNS(svgNS,"symbol");
           maximizeButtonSymbol.setAttributeNS(null,"id","maximizeButton");
           maximizeButtonSymbol.setAttributeNS(null,"overflow","visible");
           //create background rect
           var buttonRect = document.createElementNS(svgNS,"rect");
           buttonRect.setAttributeNS(null,"x",(buttonWidth / 2 * -1));
           buttonRect.setAttributeNS(null,"y",(buttonWidth / 2 * -1));
           buttonRect.setAttributeNS(null,"width",buttonWidth);
           buttonRect.setAttributeNS(null,"height",buttonWidth);
           for (var attrib in this.buttonStyles) {
               buttonRect.setAttributeNS(null,attrib,this.buttonStyles[attrib]);
           }
           buttonRect.setAttributeNS(null,"pointer-events","fill");
           maximizeButtonSymbol.appendChild(buttonRect);
           defsSection.appendChild(maximizeButtonSymbol);
       }
       this.maximizeButtonInstance = document.createElementNS(svgNS,"use");
       this.maximizeButtonInstance.setAttributeNS(null,"x",buttonPosition);
       this.maximizeButtonInstance.setAttributeNS(null,"y",this.titlebarHeight * 0.5);
       this.maximizeButtonInstance.setAttributeNS(null,"cursor","pointer");
       this.maximizeButtonInstance.setAttributeNS(null,"id","maximizeButton"+this.id);
       this.maximizeButtonInstance.setAttributeNS(xlinkNS,"href","#maximizeButton");
       this.maximizeButtonInstance.addEventListener("click",this,false);
       this.windowTitlebarGroup.appendChild(this.maximizeButtonInstance);
       buttonPosition -=  this.titlebarHeight;
   }

    //create minimize button
    if (this.minimizeButton) {
        //test if id minimizeButton exists in defs section or create a new minimizeButton symbol
        var minimizeButtonSymbol = document.getElementById("minimizeButton");
        if (!minimizeButtonSymbol) {
            var minimizeButtonSymbol = document.createElementNS(svgNS,"symbol");
            minimizeButtonSymbol.setAttributeNS(null,"id","minimizeButton");
            minimizeButtonSymbol.setAttributeNS(null,"overflow","visible");
            //create background rect
            var buttonRect = document.createElementNS(svgNS,"rect");
            buttonRect.setAttributeNS(null,"x",(buttonWidth / 2 * -1));
            buttonRect.setAttributeNS(null,"y",(buttonWidth / 2 * -1));
            buttonRect.setAttributeNS(null,"width",buttonWidth);
            buttonRect.setAttributeNS(null,"height",buttonWidth);
            buttonRect.setAttributeNS(null,"fill",this.buttonStyles["fill"]);
            buttonRect.setAttributeNS(null,"pointer-events","fill");
            minimizeButtonSymbol.appendChild(buttonRect);
            //create line
            var buttonLine = document.createElementNS(svgNS,"line");
            buttonLine.setAttributeNS(null,"x1",(buttonWidth / 2));
            buttonLine.setAttributeNS(null,"x2",(buttonWidth / 2 * -1));
            buttonLine.setAttributeNS(null,"y1",(buttonWidth / 2));
            buttonLine.setAttributeNS(null,"y2",(buttonWidth / 2));
            buttonLine.setAttributeNS(null,"stroke",this.buttonStyles["stroke"]);
            buttonLine.setAttributeNS(null,"stroke-width",this.buttonStyles["stroke-width"]);
            minimizeButtonSymbol.appendChild(buttonLine);
            defsSection.appendChild(minimizeButtonSymbol);
        }
        this.minimizeButtonInstance = document.createElementNS(svgNS,"use");
        this.minimizeButtonInstance.setAttributeNS(null,"x",buttonPosition);
        this.minimizeButtonInstance.setAttributeNS(null,"y",this.titlebarHeight * 0.5);
        this.minimizeButtonInstance.setAttributeNS(null,"cursor","pointer");
        this.minimizeButtonInstance.setAttributeNS(null,"id","minimizeButton"+this.id);
        this.minimizeButtonInstance.setAttributeNS(xlinkNS,"href","#minimizeButton");
        this.minimizeButtonInstance.addEventListener("click",this,false);
        this.windowTitlebarGroup.appendChild(this.minimizeButtonInstance);
        buttonPosition -=  this.titlebarHeight;
    }

    if (this.statusBarVisible) {
        this.statusbar = document.createElementNS(svgNS,"rect");
        this.statusbar.setAttributeNS(null,"y",(this.height - this.statusbarHeight));
        this.statusbar.setAttributeNS(null,"width",this.width);
        this.statusbar.setAttributeNS(null,"height",this.statusbarHeight);
        for (var attrib in this.statusbarStyles) {
            this.statusbar.setAttributeNS(null,attrib,this.statusbarStyles[attrib]);
        }
        this.windowMainGroup.appendChild(this.statusbar);
        this.statusTextElement = document.createElementNS(svgNS,"text");
        this.statusTextElement.setAttributeNS(null,"x",this.margin);
        this.statusTextElement.setAttributeNS(null,"y",this.height - (this.statusbarHeight - this.statustextStyles["font-size"]));
        var value = "";
        for (var attrib in this.statustextStyles) {
            value = this.statustextStyles[attrib];
            if (attrib == "font-size") {
                value += "px";
            }
            this.statusTextElement.setAttributeNS(null,attrib,value);
        }
        this.statusTextElement.setAttributeNS(null,"pointer-events","none");
        if (this.statusText.length > 0) {
            this.statusTextNode = document.createTextNode(this.statusText);
        }
        else {
            this.statusTextNode = document.createTextNode(" ");
        }
        this.statusTextElement.appendChild(this.statusTextNode);
        this.windowMainGroup.appendChild(this.statusTextElement);
    }

    //append titlebar group to window group
    this.windowGroup.appendChild(this.windowTitlebarGroup);

    //finally append group to windows group
    this.parentGroup.appendChild(this.windowGroup);

    //issue event that window was created
    this.timer.setTimeout("fireFunction",this.timerMs,"created");    
    }
    else {
        alert("could not create or reference 'parentNode' of window with id '"+this.id+"'");
    }
}

//test if window group exists or create a new group at the end of the file
Window.prototype.testParent = function() {
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

//central mouse-event handling
Window.prototype.handleEvent = function(evt) {
    if (evt.type == "click") {
        var elId = evt.currentTarget.getAttributeNS(null,"id");
        if (elId == "closeButton"+this.id) {
            this.close(true);
        }
        if (elId == "maximizeButton"+this.id) {
            this.maximize(true);
        }
        if (elId == "minimizeButton"+this.id) {
            this.minimize(true);
        }
        if (elId == "titleBar"+this.id || elId == "decoGroup"+this.id) {
            if (evt.detail == 2) {
                if (this.minimized) {
                    this.maximize(true);
                }
                else {
                    this.minimize(true);
                }
            }
        }
    }
    if (evt.type == "mousedown") {
        var elId = evt.currentTarget.getAttributeNS(null,"id");
        if (elId == "titleBar"+this.id || elId == "decoGroup"+this.id) {
            //put it to the front
            this.parentGroup.appendChild(this.windowGroup);
            if (this.moveable) {
                this.panStatus = 1;
                //var coords = myMapApp.calcCoord(evt,document.documentElement);
                var coords = myMapApp.calcCoord(evt,this.parentGroup);
                this.panCoords = coords;
                document.documentElement.addEventListener("mousemove",this,false);
                document.documentElement.addEventListener("mouseup",this,false);
                if (!this.showContent) {
                    this.windowTitlebarGroup.setAttributeNS(null,"display","none");
                    this.windowMainGroup.setAttributeNS(null,"display","none");
                    if (this.minimized) {
                        this.shadowTitleRect.setAttributeNS(null,"display","inherit");
                    }
                    else {
                        this.shadowRect.setAttributeNS(null,"display","inherit");
                    }
                }
                else {
                     if (this.titleBarVisible) {
                           this.titlebar.setAttributeNS(null,"cursor","move");
                    }
                    if (this.decorationGroup) {
                           this.decorationGroup.setAttributeNS(null,"cursor","move");
                    }
                }
                this.windowGroup.setAttributeNS(batikNS,"static","true");
                this.fireFunction("moveStart");
            }
        }
    }
    if (evt.type == "mousemove") {
        if (this.panStatus == 1) {
            //var coords = myMapApp.calcCoord(evt,document.documentElement);
            var coords = myMapApp.calcCoord(evt,this.parentGroup);
            if (coords.x < this.constrXmin || coords.x > this.constrXmax || coords.y < this.constrYmin || coords.y > this.constrYmax) {
                this.stopDrag();
            }
            else {
                this.transX += coords.x - this.panCoords.x;
                this.transY += coords.y - this.panCoords.y;
                //check constraints
                if (this.transX < this.constrXmin) {
                    this.transX = this.constrXmin;
                }
                if (this.transY < this.constrYmin) {
                    this.transY = this.constrYmin;
                }
                if ((this.transX + this.width) > (this.constrXmax)) {
                    this.transX = this.constrXmax - this.width;
                }
                if (this.minimized) {
                    if ((this.transY + this.titlebarHeight) > (this.constrYmax)) {
                        this.transY = this.constrYmax - this.titlebarHeight;
                    }
                }
                else {
                    if ((this.transY + this.height) > (this.constrYmax)) {
                        this.transY = this.constrYmax - this.height;
                    }
                }
                this.windowGroup.setAttributeNS(null,"transform","translate("+this.transX+","+this.transY+")");
                this.panCoords = coords;
                this.timer.setTimeout("fireFunction",this.timerMs,"moved");
            }
        }
    }
    if (evt.type == "mouseup") {
        if (this.panStatus == 1) {
            this.stopDrag();
        }
    }
}

Window.prototype.fireFunction = function(evtType) {
    if (typeof(this.functionToCall) == "function") {
        this.functionToCall(this.id,evtType);
    }
    if (typeof(this.functionToCall) == "object") {
        this.functionToCall.windowStatusChanged(this.id,evtType);
    }
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}

//helper method to stop the dragging mode
Window.prototype.stopDrag = function() {
        this.windowGroup.removeAttributeNS(batikNS,"static");
        document.documentElement.removeEventListener("mousemove",this,false);
        document.documentElement.removeEventListener("mouseup",this,false);
        if (!this.showContent) {
            this.windowTitlebarGroup.setAttributeNS(null,"display","inherit");
            if (this.minimized) {
                this.shadowTitleRect.setAttributeNS(null,"display","none");
            }
            else {
                this.shadowRect.setAttributeNS(null,"display","none");
                this.windowMainGroup.setAttributeNS(null,"display","inherit");
            }
        }
        else {
            if (this.titleBarVisible) {
                this.titlebar.setAttributeNS(null,"cursor","pointer");
            }
            if (this.decorationGroup) {
                this.decorationGroup.setAttributeNS(null,"cursor","pointer");
            }
        }
        this.timer.setTimeout("fireFunction",this.timerMs,"moveEnd");
        this.panStatus = 0;
}

//minimize a window
Window.prototype.minimize = function(fireFunction) {
    this.windowMainGroup.setAttributeNS(null,"display","none");
    if (this.decorationGroupMinimized) {
        this.decorationGroupMinimized.setAttributeNS(null,"display","none");
    }
    this.minimized = true;
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs,"minimized");
    }
}

//maximize a window
Window.prototype.maximize = function(fireFunction) {
    this.windowMainGroup.setAttributeNS(null,"display","inherit");
    if (this.decorationGroupMinimized) {
        this.decorationGroupMinimized.setAttributeNS(null,"display","inherit");
    }
    if ((this.transY + this.height) > (this.constrYmax)) {
        this.transY = this.constrYmax - this.height;
        this.windowGroup.setAttributeNS(null,"transform","translate("+this.transX+","+this.transY+")");
    }
    this.minimized = false;
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs,"maximized");
    }
}

//open a closed window
Window.prototype.close = function(fireFunction) {
    this.windowGroup.setAttributeNS(null,"display","none");
    this.closed = true;
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs,"closed");
    }
}

//close window, after closing the window is still in its previous state and can be re-opened
Window.prototype.open = function(fireFunction) {
    if (!this.removed) {
        this.windowGroup.setAttributeNS(null,"display","inherit");
        this.closed = false;
        if (fireFunction) {
            this.timer.setTimeout("fireFunction",this.timerMs,"opened");
        }
    }
    else {
        alert("window " + this.id + " is already removed");
    }
}

//resize window and reposition it into constrained coords
Window.prototype.resize = function(width,height,fireFunction) {
    this.width = width;
    this.height = height;
    //adopt shadow rect
    this.shadowRect.setAttributeNS(null,"width",this.width);
    this.shadowRect.setAttributeNS(null,"height",this.height);
    this.shadowTitleRect.setAttributeNS(null,"width",this.width);
    //adopt background rect
    this.backgroundRect.setAttributeNS(null,"width",this.width);
    this.backgroundRect.setAttributeNS(null,"height",this.height);
    //adopt titlebar
    if (this.titleBarVisible) {
        this.titlebar.setAttributeNS(null,"width",this.width);
    }
    var buttonPosition = this.width - this.margin - this.titlebarHeight * 0.5;
    if (this.closeButton) {
        this.closeButtonInstance.setAttributeNS(null,"x",buttonPosition);
        buttonPosition -=  this.titlebarHeight;
    }
    if (this.maximizeButton) {
        this.maximizeButtonInstance.setAttributeNS(null,"x",buttonPosition);
        buttonPosition -=  this.titlebarHeight;
    }
    if (this.minimizeButton) {
        this.minimizeButtonInstance.setAttributeNS(null,"x",buttonPosition);
    }
    if (this.statusBarVisible) {
        this.statusbar.setAttributeNS(null,"y",(this.height - this.statusbarHeight));
        this.statusbar.setAttributeNS(null,"width",this.width);    
        this.statusTextElement.setAttributeNS(null,"y",this.height - (this.statusbarHeight - this.statustextStyles["font-size"]));
    }
    //check constraints
    if (this.transX < this.constrXmin) {
        this.transX = this.constrXmin;
    }
    if (this.transY < this.constrYmin) {
        this.transY = this.constrYmin;
    }
    if ((this.transX + this.width) > (this.constrXmax)) {
        this.transX = this.constrXmax - this.width;
    }
    if (this.minimized) {
        if ((this.transY + this.titlebarHeight) > (this.constrYmax)) {
            this.transY = this.constrYmax - this.titlebarHeight;
        }
    }
    else {
        if ((this.transY + this.height) > (this.constrYmax)) {
            this.transY = this.constrYmax - this.height;
        }
    }
    this.windowGroup.setAttributeNS(null,"transform","translate("+this.transX+","+this.transY+")");
    if (fireFunction) {
        this.fireFunction("resized");
    }
}

//remove window
Window.prototype.remove = function(fireFunction) {
    if (!this.removed) {
        this.windowGroup.parentGroup.removeChild(this.windowGroup);
        this.removed = true;
        if (fireFunction) {
            this.timer.setTimeout("fireFunction",this.timerMs,"removed");
        }
    }
}

//change content of statusBar
Window.prototype.setStatusText = function(statusText) {
    if (this.statusBarVisible) {
        this.statusText = statusText;
        if (this.statusText.length > 0) {
            this.statusTextNode.nodeValue = this.statusText;
        }
        else {
            this.statusTextNode.nodeValue = " ";
        }

    }
    else {
        alert("there is no statusbar available");
    }
}

//change content of statusBar
Window.prototype.setTitleText = function(titleText) {
    this.titleText = titleText;
    if (titleText.length > 0) {
        this.titleTextNode.nodeValue = titleText;
    }
    else {
        this.titleTextNode.nodeValue = " ";
    }
}

//move a window to a certain position (upper left corner)
Window.prototype.moveTo = function(coordx,coordy,fireFunction) {
    this.transX = coordx;
    this.transY = coordy;
    //check constraints
    if (this.transX < this.constrXmin) {
        this.transX = this.constrXmin;
    }
    if (this.transY < this.constrYmin) {
        this.transY = this.constrYmin;
    }
    if ((this.transX + this.width) > (this.constrXmax)) {
        this.transX = this.constrXmax - this.width;
    }
    if (this.minimized) {
        if ((this.transY + this.titlebarHeight) > (this.constrYmax)) {
            this.transY = this.constrYmax - this.titlebarHeight;
        }
    }
    else {
        if ((this.transY + this.height) > (this.constrYmax)) {
            this.transY = this.constrYmax - this.height;
        }
    }
    this.windowGroup.setAttributeNS(null,"transform","translate("+this.transX+","+this.transY+")");
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs,"movedTo");
    }
}

//append new content to the window main group
Window.prototype.appendContent = function(node,inheritDisplay) {
    if (typeof(node) == "string") {
        node = document.getElementById(node);
    }
    if (inheritDisplay) {
        node.setAttributeNS(null,"display","inherit");
    }
    this.windowContentGroup.appendChild(node);
}

//remove new content from the window main group
Window.prototype.removeContent = function(node) {
    if (typeof(node) == "string") {
        node = document.getElementById(node);
    }
    this.windowContentGroup.removeChild(node);
}

//remove new content from the window main group
Window.prototype.insertContentBefore = function(node,referenceNode,inheritDisplay) {
    if (typeof(node) == "string") {
        node = document.getElementById(node);
    }
    if (typeof(referenceNode) == "string") {
        referenceNode = document.getElementById(referenceNode);
    }
    if (inheritDisplay) {
        node.setAttributeNS(null,"display","inherit");
    }
    this.windowContentGroup.insertBefore(node,referenceNode);
}

//hide content of Window
Window.prototype.hideContents = function() {
    this.windowContentGroup.setAttributeNS(null,"display","none");
}

//show content of Window
Window.prototype.showContents = function() {
    this.windowContentGroup.setAttributeNS(null,"display","inherit");
}

//add window decoration
Window.prototype.addWindowDecoration = function(node,mayTriggerMoving,topOrBottom) {
    if (typeof(node) == "string") {
        node = document.getElementById(node);
    }
    if (this.decorationGroup) {
        var parent = this.decorationGroup.parentNode;
        parent.removeChild(this.decorationGroup);
    }
    if (topOrBottom == "bottom") {
        this.decorationGroup = this.windowGroup.insertBefore(node,this.windowGroup.firstChild);
    }    
    else if (topOrBottom == "top") {
        this.decorationGroup = this.windowTitlebarGroup.insertBefore(node,this.windowTitlebarGroup.firstChild);
    }
    else {
        alert("Error in window with id '"+this.id+"': you have to specify 'top' or 'bottom' for the variable 'topOrBottom'.");
    }
    if (mayTriggerMoving) {
        this.decorationGroup.setAttributeNS(null,"id","decoGroup"+this.id);
        this.decorationGroup.setAttributeNS(null,"cursor","pointer");
        this.decorationGroup.addEventListener("click",this,false);
        this.decorationGroup.addEventListener("mousedown",this,false);
    }
    //see if there is a sub group/element that should be hidden when minimized
    try {
        this.decorationGroupMinimized = document.getElementById("decoGroupMinimized"+this.id);
        if (this.minimized) {
            this.decorationGroupMinimized.setAttributeNS(null,"display","none");
        }
    }
    catch(er) {
        this.decorationGroupMinimized = null;
    }
}
