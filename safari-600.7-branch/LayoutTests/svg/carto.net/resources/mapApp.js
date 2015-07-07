/*
Scripts for creating SVG apps, converting clientX/Y to viewBox coordinates
and for displaying tooltips

Copyright (C) <2006>  <Andreas Neumann>
Version 1.2, 2006-10-06
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:
* thanks to Kevin Lindsey for his many examples and providing the ViewBox class

----

Documentation: http://www.carto.net/papers/svg/gui/mapApp/

----

current version: 1.2

version history:
1.0 (2006-06-01)
initial version
Was programmed earlier, but now documented

1.1 (2006-06-15)
added properties this.innerWidth, this.innerHeight (wrapper around different behaviour of viewers), added method ".adjustViewBox()" to adjust the viewBox to the this.innerWidth and this.innerHeight of the UA's window

1.2 (2006-10-06)
added two new constructor parameter "adjustVBonWindowResize" and "resizeCallbackFunction". If the first parameter is set to true, the viewBox of this mapApp will always adjust itself to the innerWidth and innerHeight of the browser window or frame containing the SVG application
the "resizeCallbackFunction" can be of type "function", later potentially also of type "object". This function is called every time the mapApp was resized (browser/UA window was resized). It isn't called the first time when the mapApp was initialized
added a new way to detect resize events in Firefox which didn't implement the SVGResize event so far
added several arrays to hold GUI references

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

original document site: http://www.carto.net/papers/svg/gui/mapApp/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this code, please link to the original (http://www.carto.net/papers/svg/gui/mapApp/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

//this mapApp object helps to convert clientX/clientY coordinates to the coordinates of the group where the element is within
//normally one can just use .getScreenCTM(), but ASV3 does not implement it, 95% of the code in this function is for ASV3!!!
//credits: Kevin Lindsey for his example at http://www.kevlindev.com/gui/utilities/viewbox/ViewBox.js
function mapApp(adjustVBonWindowResize,resizeCallbackFunction) {
    this.adjustVBonWindowResize = adjustVBonWindowResize;
    this.resizeCallbackFunction = resizeCallbackFunction;
    this.initialized = false;
    if (!document.documentElement.getScreenCTM) {
        //add zoom and pan event event to document element
        //this part is only required for viewers not supporting document.documentElement.getScreenCTM() (e.g. ASV3)
        document.documentElement.addEventListener("SVGScroll",this,false);
        document.documentElement.addEventListener("SVGZoom",this,false);
    }
    //add SVGResize event, note that because FF does not yet support the SVGResize event, there is a workaround
     try {
          //browsers with native SVG support
          window.addEventListener("resize",this,false);
     }
    catch(er) {
        //SVG UAs, like Batik and ASV/Iex
        document.documentElement.addEventListener("SVGResize",this,false);
    }
    //determine the browser main version
    this.navigator = "Batik";
    if (window.navigator) {
        if (window.navigator.appName.match(/Adobe/gi)) {
            this.navigator = "Adobe";
        }
        if (window.navigator.appName.match(/Netscape/gi)) {
            this.navigator = "Mozilla";
        }
        if (window.navigator.appName.match(/Opera/gi)) {
            this.navigator = "Opera";
        }
        if (window.navigator.appName.match(/Safari/gi)) {
            this.navigator = "Safari";
        }
    }
    //we need to call this once to initialize this.innerWidth/this.innerHeight
    this.resetFactors();
    //per default, tooltips are disabled
    this.tooltipsEnabled = false;
    //create new arrays to hold GUI references
    this.Windows = new Array();
    this.checkBoxes = new Array();
    this.radioButtonGroups = new Array();
    this.tabgroups = new Array();
    this.textboxes = new Array();
    this.buttons = new Array();    
    this.selectionLists = new Array();    
    this.comboboxes = new Array();    
    this.sliders = new Array();
    this.scrollbars = new Array();
    this.colourPickers = new Array();
}

mapApp.prototype.handleEvent = function(evt) {
    if (evt.type == "SVGResize" || evt.type == "resize" || evt.type == "SVGScroll" || evt.type == "SVGZoom") {
        this.resetFactors();
    }
    if ((evt.type == "mouseover" || evt.type == "mouseout" || evt.type == "mousemove") && this.tooltipsEnabled) {
        this.displayTooltip(evt);
    }
}

mapApp.prototype.resetFactors = function() {
    //set inner width and height
    if (window.innerWidth) {
        this.innerWidth = window.innerWidth;
        this.innerHeight = window.innerHeight;
    }
    else {
        var viewPort = document.documentElement.viewport;
        this.innerWidth = viewPort.width;
        this.innerHeight = viewPort.height;
    }
    if (this.adjustVBonWindowResize) {
        this.adjustViewBox();
    }
    //this code is for ASV3
    if (!document.documentElement.getScreenCTM) {
        var svgroot = document.documentElement;
        this.viewBox = new ViewBox(svgroot);
        var trans = svgroot.currentTranslate;
        var scale = svgroot.currentScale;
        this.m = this.viewBox.getTM();
        //undo effects of zoom and pan
        this.m = this.m.scale( 1/scale );
        this.m = this.m.translate(-trans.x, -trans.y);
    }
    if (this.resizeCallbackFunction && this.initialized) {
        if (typeof(this.resizeCallbackFunction) == "function") {
            this.resizeCallbackFunction();
        }
    }
    this.initialized = true;
}

//set viewBox of document.documentElement to innerWidth and innerHeight
mapApp.prototype.adjustViewBox = function() {
    document.documentElement.setAttributeNS(null,"viewBox","0 0 "+this.innerWidth+" "+this.innerHeight);
}

mapApp.prototype.calcCoord = function(evt,ctmNode) {
    var svgPoint = document.documentElement.createSVGPoint();
    svgPoint.x = evt.clientX;
    svgPoint.y = evt.clientY;
    if (!document.documentElement.getScreenCTM) {
        //undo the effect of transformations
        if (ctmNode) {
            var matrix = getTransformToRootElement(ctmNode);
        }
        else {
            var matrix = getTransformToRootElement(evt.target);            
        }
          svgPoint = svgPoint.matrixTransform(matrix.inverse().multiply(this.m));
    }
    else {
        //case getScreenCTM is available
        if (ctmNode) {
            var matrix = ctmNode.getScreenCTM();
        }
        else {
            var matrix = evt.target.getScreenCTM();        
        }
      svgPoint = svgPoint.matrixTransform(matrix.inverse());
    }
  //undo the effect of viewBox and zoomin/scroll
    return svgPoint;
}

mapApp.prototype.calcInvCoord = function(svgPoint) {
    if (!document.documentElement.getScreenCTM) {
        var matrix = getTransformToRootElement(document.documentElement);
    }
    else {
        var matrix = document.documentElement.getScreenCTM();
    }
    svgPoint = svgPoint.matrixTransform(matrix);
    return svgPoint;
}

//initialize tootlips
mapApp.prototype.initTooltips = function(groupId,tooltipTextAttribs,tooltipRectAttribs,xOffset,yOffset,padding) {
    var nrArguments = 6;
    if (arguments.length == nrArguments) {
        this.toolTipGroup = document.getElementById(groupId);
        this.tooltipTextAttribs = tooltipTextAttribs;
        if (!this.tooltipTextAttribs["font-size"]) {
            this.tooltipTextAttribs["font-size"] = 12;
        }    
        this.tooltipRectAttribs = tooltipRectAttribs;
        this.xOffset = xOffset;
        this.yOffset = yOffset;
        this.padding = padding;
        if (!this.toolTipGroup) {
            alert("Error: could not find tooltip group with id '"+groupId+"'. Please specify a correct tooltip parent group id!");
        }
        else {
            //set tooltip group to invisible
            this.toolTipGroup.setAttributeNS(null,"visibility","hidden");
            this.toolTipGroup.setAttributeNS(null,"pointer-events","none");
            this.tooltipsEnabled = true;
            //create tooltip text element
            this.tooltipText = document.createElementNS(svgNS,"text");
            for (var attrib in this.tooltipTextAttribs) {
                value = this.tooltipTextAttribs[attrib];
                if (attrib == "font-size") {
                    value += "px";
                }
                this.tooltipText.setAttributeNS(null,attrib,value);
            }
            //create textnode
            var textNode = document.createTextNode("Tooltip");
            this.tooltipText.appendChild(textNode);
            this.toolTipGroup.appendChild(this.tooltipText);
            var bbox = this.tooltipText.getBBox();
            this.tooltipRect = document.createElementNS(svgNS,"rect");
            this.tooltipRect.setAttributeNS(null,"x",bbox.x-this.padding);
            this.tooltipRect.setAttributeNS(null,"y",bbox.y-this.padding);
            this.tooltipRect.setAttributeNS(null,"width",bbox.width+this.padding*2);
            this.tooltipRect.setAttributeNS(null,"height",bbox.height+this.padding*2);
            for (var attrib in this.tooltipRectAttribs) {
                this.tooltipRect.setAttributeNS(null,attrib,this.tooltipRectAttribs[attrib]);
            }
            this.toolTipGroup.insertBefore(this.tooltipRect,this.tooltipText);
        }
    }
    else {
            alert("Error in method 'initTooltips': wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");            
    }
}

mapApp.prototype.addTooltip = function(tooltipNode,tooltipTextvalue,followmouse,checkForUpdates,targetOrCurrentTarget,childAttrib) {
    var nrArguments = 6;
    if (arguments.length == nrArguments) {
        //get reference
        if (typeof(tooltipNode) == "string") {
            tooltipNode = document.getElementById(tooltipNode);
        }
        //check if tooltip attribute present or create one
        if (!tooltipNode.hasAttributeNS(attribNS,"tooltip")) {
            if (tooltipTextvalue) {
                tooltipNode.setAttributeNS(attribNS,"tooltip",tooltipTextvalue);
            }
            else {
                tooltipNode.setAttributeNS(attribNS,"tooltip","Tooltip");            
            }
        }
        //see if we need updates
        if (checkForUpdates) {
            tooltipNode.setAttributeNS(attribNS,"tooltipUpdates","true");        
        }
        //see if we have to use evt.target
        if (targetOrCurrentTarget == "target") {
            tooltipNode.setAttributeNS(attribNS,"tooltipParent","true");
        }
        //add childAttrib
        if (childAttrib) {
            tooltipNode.setAttributeNS(attribNS,"tooltipAttrib",childAttrib);
        }
        //add event listeners
        tooltipNode.addEventListener("mouseover",this,false);
        tooltipNode.addEventListener("mouseout",this,false);
        if (followmouse) {
            tooltipNode.addEventListener("mousemove",this,false);
        }
    }
    else {
        alert("Error in method 'addTooltip()': wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");            
    }
}

mapApp.prototype.displayTooltip = function(evt) {
    var curEl = evt.currentTarget;
    var coords = this.calcCoord(evt,this.toolTipGroup.parentNode);
    if (evt.type == "mouseover") {
        this.toolTipGroup.setAttributeNS(null,"visibility","visible");
        this.toolTipGroup.setAttributeNS(null,"transform","translate("+(coords.x+this.xOffset)+","+(coords.y+this.yOffset)+")");
        this.updateTooltip(evt);
    }
    if (evt.type == "mouseout") {
        this.toolTipGroup.setAttributeNS(null,"visibility","hidden");
    }
    if (evt.type == "mousemove") {
        this.toolTipGroup.setAttributeNS(null,"transform","translate("+(coords.x+this.xOffset)+","+(coords.y+this.yOffset)+")");        
        if (curEl.hasAttributeNS(attribNS,"tooltipUpdates")) {
            this.updateTooltip(evt);
        }
    }
}

mapApp.prototype.updateTooltip = function(evt) {
    var el = evt.currentTarget;
    if (el.hasAttributeNS(attribNS,"tooltipParent")) {
        var attribName = "tooltip";
        if (el.hasAttributeNS(attribNS,"tooltipAttrib")) {
            attribName = el.getAttributeNS(attribNS,"tooltipAttrib");
        }
        el = evt.target;
        var myText = el.getAttributeNS(attribNS,attribName);
    }
    else {
        var myText = el.getAttributeNS(attribNS,"tooltip");
    }
    var textArray = myText.split("\\n");
    while(this.tooltipText.hasChildNodes()) {
        this.tooltipText.removeChild(this.tooltipText.lastChild);
    }
    for (var i=0;i<textArray.length;i++) {
        var tspanEl = document.createElementNS(svgNS,"tspan");
        tspanEl.setAttributeNS(null,"x",0);
        var dy = this.tooltipTextAttribs["font-size"];
        if (i == 0) {
            var dy = 0;
        }
        tspanEl.setAttributeNS(null,"dy",dy);
        var textNode = document.createTextNode(textArray[i]);
        tspanEl.appendChild(textNode);
        this.tooltipText.appendChild(tspanEl);
    }
    // set text and rect attributes
    var bbox = this.tooltipText.getBBox();
    this.tooltipRect.setAttributeNS(null,"x",bbox.x-this.padding);
    this.tooltipRect.setAttributeNS(null,"y",bbox.y-this.padding);
    this.tooltipRect.setAttributeNS(null,"width",bbox.width+this.padding*2);
    this.tooltipRect.setAttributeNS(null,"height",bbox.height+this.padding*2);    
}

mapApp.prototype.enableTooltips = function() {
    this.tooltipsEnabled = true;
}

mapApp.prototype.disableTooltips = function() {
    this.tooltipsEnabled = false;
    this.toolTipGroup.setAttributeNS(null,"visibility","hidden");
}

/*************************************************************************/

/*****
*
*   ViewBox.js
*
*   copyright 2002, Kevin Lindsey
*
*****/

ViewBox.VERSION = "1.0";


/*****
*
*   constructor
*
*****/
function ViewBox(svgNode) {
    if ( arguments.length > 0 ) {
        this.init(svgNode);
    }
}


/*****
*
*   init
*
*****/
ViewBox.prototype.init = function(svgNode) {
    var viewBox = svgNode.getAttributeNS(null, "viewBox");
    var preserveAspectRatio = svgNode.getAttributeNS(null, "preserveAspectRatio");
    
    if ( viewBox != "" ) {
        var params = viewBox.split(/\s*,\s*|\s+/);

        this.x      = parseFloat( params[0] );
        this.y      = parseFloat( params[1] );
        this.width  = parseFloat( params[2] );
        this.height = parseFloat( params[3] );
    } else {
        this.x      = 0;
        this.y      = 0;
        this.width  = innerWidth;
        this.height = innerHeight;
    }
    
    this.setPAR(preserveAspectRatio);
    var dummy = this.getTM(); //to initialize this.windowWidth/this.windowHeight
};


/*****
*
*   getTM
*
*****/
ViewBox.prototype.getTM = function() {
    var svgRoot      = document.documentElement;
    var matrix       = document.documentElement.createSVGMatrix();
        //case width/height contains percent
    this.windowWidth = svgRoot.getAttributeNS(null,"width");
    if (this.windowWidth.match(/%/) || this.windowWidth == null) {
        if (this.windowWidth == null) {
            if (window.innerWidth) {
                this.windowWidth = window.innerWidth;
            }
            else {
                this.windowWidth = svgRoot.viewport.width;
            }
        }
        else {
            var factor = parseFloat(this.windowWidth.replace(/%/,""))/100;
            if (window.innerWidth) {
                this.windowWidth = window.innerWidth * factor;
            }
            else {
                this.windowWidth = svgRoot.viewport.width * factor;
            }
        }
    }
    else {
        this.windowWidth = parseFloat(this.windowWidth);
    }
    this.windowHeight = svgRoot.getAttributeNS(null,"height");
    if (this.windowHeight.match(/%/) || this.windowHeight == null) {
        if (this.windowHeight == null) {
            if (window.innerHeight) {
                this.windowHeight = window.innerHeight;
            }
            else {
                this.windowHeight = svgRoot.viewport.height;
            }
        }
        else {
            var factor = parseFloat(this.windowHeight.replace(/%/,""))/100;
            if (window.innerHeight) {
                this.windowHeight = window.innerHeight * factor;
            }
            else {
                this.windowHeight = svgRoot.viewport.height * factor;
            }
        }
    }
    else {
        this.windowHeight = parseFloat(this.windowHeight);
    }
    var x_ratio = this.width  / this.windowWidth;
    var y_ratio = this.height / this.windowHeight;

    matrix = matrix.translate(this.x, this.y);
    if ( this.alignX == "none" ) {
        matrix = matrix.scaleNonUniform( x_ratio, y_ratio );
    } else {
        if ( x_ratio < y_ratio && this.meetOrSlice == "meet" ||
             x_ratio > y_ratio && this.meetOrSlice == "slice"   )
        {
            var x_trans = 0;
            var x_diff  = this.windowWidth*y_ratio - this.width;

            if ( this.alignX == "Mid" )
                x_trans = -x_diff/2;
            else if ( this.alignX == "Max" )
                x_trans = -x_diff;
            
            matrix = matrix.translate(x_trans, 0);
            matrix = matrix.scale( y_ratio );
        }
        else if ( x_ratio > y_ratio && this.meetOrSlice == "meet" ||
                  x_ratio < y_ratio && this.meetOrSlice == "slice"   )
        {
            var y_trans = 0;
            var y_diff  = this.windowHeight*x_ratio - this.height;

            if ( this.alignY == "Mid" )
                y_trans = -y_diff/2;
            else if ( this.alignY == "Max" )
                y_trans = -y_diff;
            
            matrix = matrix.translate(0, y_trans);
            matrix = matrix.scale( x_ratio );
        }
        else
        {
            // x_ratio == y_ratio so, there is no need to translate
            // We can scale by either value
            matrix = matrix.scale( x_ratio );
        }
    }

    return matrix;
}


/*****
*
*   get/set methods
*
*****/

/*****
*
*   setPAR
*
*****/
ViewBox.prototype.setPAR = function(PAR) {
    // NOTE: This function needs to use default values when encountering
    // unrecognized values
    if ( PAR ) {
        var params = PAR.split(/\s+/);
        var align  = params[0];

        if ( align == "none" ) {
            this.alignX = "none";
            this.alignY = "none";
        } else {
            this.alignX = align.substring(1,4);
            this.alignY = align.substring(5,9);
        }

        if ( params.length == 2 ) {
            this.meetOrSlice = params[1];
        } else {
            this.meetOrSlice = "meet";
        }
    } else {
        this.align  = "xMidYMid";
        this.alignX = "Mid";
        this.alignY = "Mid";
        this.meetOrSlice = "meet";
    }
};
