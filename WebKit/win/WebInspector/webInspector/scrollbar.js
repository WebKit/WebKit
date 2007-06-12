/*
© Copyright 2005 Apple Computer, Inc. All rights reserved.

IMPORTANT:  This Apple software and the associated images located in
/System/Library/WidgetResources/AppleClasses/ (collectively "Apple Software")
are supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your
agreement to the following terms. Your use, installation and/or redistribution
of this Apple Software constitutes acceptance of these terms. If you do not
agree with these terms, please do not use, install, or redistribute this Apple
Software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under
Apple’s copyrights in the Apple Software, to use, reproduce, and redistribute
the Apple Software, in text form (for JavaScript files) or binary form (for
associated images), for the sole purpose of creating Dashboard widgets for Mac
OS X.

If you redistribute the Apple Software, you must retain this entire notice and
the warranty disclaimers and limitation of liability provisions (last two
paragraphs below) in all such redistributions of the Apple Software.

You may not use the name, trademarks, service marks or logos of Apple to endorse
or promote products that include the Apple Software without the prior written
permission of Apple. Except as expressly stated in this notice, no other rights
or licenses, express or implied, are granted by Apple herein, including but not
limited to any patent rights that may be infringed by your products that
incorporate the Apple Software or by other works in which the Apple Software may
be incorporated.

The Apple Software is provided on an "AS IS" basis.  APPLE MAKES NO WARRANTIES,
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
REGARDING THE APPPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION
WITH YOUR PRODUCTS.

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, AND/OR DISTRIBUTION OF THE
APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

function Scrollbar(scrollbar)
{
}

/*
 * _init() member function
 * Initialize the scrollbar.
 * pre: this.scrollbar
 * post: this._thumb, this._track + event handlers
 */
Scrollbar.prototype._init = function()
{
	var style = null;
	var element = null;
		
	// Scrollbar Track
	this._track = document.createElement("div");
	style = this._track.style;
	// fill our containing div
	style.height = "100%";
	style.width = "100%";
	this.scrollbar.appendChild(this._track);
	
	// Scrollbar Track Top
	element = document.createElement("div");
	element.style.position = "absolute";
	this._setObjectStart(element, 0);
	this._track.appendChild(element);
	
	// Scrollbar Track Middle
	element = document.createElement("div");
	element.style.position = "absolute";
	this._track.appendChild(element);
	
	// Scrollbar Track Bottom
	element = document.createElement("div");
	element.style.position = "absolute";
	this._setObjectEnd(element, 0);
	this._track.appendChild(element);
	
	// Scrollbar Thumb
	this._thumb = document.createElement("div");
	style = this._thumb.style;
	style.position = "absolute";
	this._setObjectSize(this._thumb, this.minThumbSize); // default size
	this._track.appendChild(this._thumb);
	
	// Scrollbar Thumb Top
	element = document.createElement("div");
	element.style.position = "absolute";
	this._setObjectStart(element, 0);
	this._thumb.appendChild(element);
	
	// Scrollbar Thumb Middle
	element = document.createElement("div");
	element.style.position = "absolute";
	this._thumb.appendChild(element);
	
	// Scrollbar Thumb Bottom
	element = document.createElement("div");
	element.style.position = "absolute";
	this._setObjectEnd(element, 0);
	this._thumb.appendChild(element);
	
	// Set up styles
	this.setSize(this.size);
	
	this.setTrackStart(this.trackStartPath, this.trackStartLength);
	this.setTrackMiddle(this.trackMiddlePath);
	this.setTrackEnd(this.trackEndPath, this.trackEndLength);
	this.setThumbStart(this.thumbStartPath, this.thumbStartLength);
	this.setThumbMiddle(this.thumbMiddlePath);
	this.setThumbEnd(this.thumbEndPath, this.thumbEndLength);
	
	// hide the thumb until we refresh
	this._thumb.style.display = "none";
	
	// Add event listeners
	this._track.addEventListener("mousedown", this._mousedownTrackHandler, false);
	this._thumb.addEventListener("mousedown", this._mousedownThumbHandler, false);
	
	// ScrollArea will fire a refresh for us
}

Scrollbar.prototype.remove = function()
{
	this.scrollbar.removeChild(this._track);
}

// Capture events that we don't handle but also don't want getting through
Scrollbar.prototype._captureEvent = function(event)
{
	event.stopPropagation();
	event.preventDefault();
}

/*********************
 * Thumb scroll events
 */
Scrollbar.prototype._mousedownThumb = function(event)
{
	// temporary event listeners
	document.addEventListener("mousemove", this._mousemoveThumbHandler, true);
	document.addEventListener("mouseup", this._mouseupThumbHandler, true);
	document.addEventListener("mouseover", this._captureEventHandler, true);
	document.addEventListener("mouseout", this._captureEventHandler, true);
	
	this._thumbStart_temp = this._getMousePosition(event);
	
	this._scroll_thumbStartPos = this._getThumbStartPos();
	
	event.stopPropagation();
	event.preventDefault();
}

Scrollbar.prototype._mousemoveThumb = function(event)
{
	var delta = this._getMousePosition(event) - this._thumbStart_temp;
	
	var new_pos = this._scroll_thumbStartPos + delta;
	this.scrollTo(this._contentPositionForThumbPosition(new_pos));
	
	event.stopPropagation();
	event.preventDefault();
}

Scrollbar.prototype._mouseupThumb = function(event)
{
	// remove temporary event handlers
	document.removeEventListener("mousemove", this._mousemoveThumbHandler, true);
	document.removeEventListener("mouseup", this._mouseupThumbHandler, true);
	document.removeEventListener("mouseover", this._captureEventHandler, true);
	document.removeEventListener("mouseout", this._captureEventHandler, true);
	
	// remove temporary properties
	delete this._thumbStart_temp;
	delete this._scroll_thumbStartPos;
	
	event.stopPropagation();
	event.preventDefault();
}

/*********************
 * Track scroll events
 */
Scrollbar.prototype._mousedownTrack = function(event)
{
	this._track_mouse_temp = this._getMousePosition(event) - this._trackOffset;
	
	if (event.altKey)
	{
		this.scrollTo(this._contentPositionForThumbPosition(this._track_mouse_temp - (this._thumbLength / 2)));
		delete this._track_mouse_temp;
	}
	else
	{
		this._track_scrolling = true;
		
		// temporary event handlers
		this._track.addEventListener("mousemove", this._mousemoveTrackHandler, true);
		this._track.addEventListener("mouseover", this._mouseoverTrackHandler, true);
		this._track.addEventListener("mouseout", this._mouseoutTrackHandler, true);
		document.addEventListener("mouseup", this._mouseupTrackHandler, true);
		
		this._trackScrollOnePage(this);
		this._track_timer = setInterval(this._trackScrollDelay, 500, this);
	}
	
	event.stopPropagation();
	event.preventDefault();
}

Scrollbar.prototype._trackScrollDelay = function(self)
{
	if (!self._track_scrolling) return;
	
	clearInterval(self._track_timer);
	
	self._trackScrollOnePage(self);
	self._track_timer = setInterval(self._trackScrollOnePage, 150, self);
}

Scrollbar.prototype._mousemoveTrack = function(event)
{
	this._track_mouse_temp = this._getMousePosition(event) - this._trackOffset;
	
	event.stopPropagation();
	event.preventDefault();
}

Scrollbar.prototype._mouseoverTrack = function(event)
{
	this._track_mouse_temp = this._getMousePosition(event) - this._trackOffset;
	this._track_scrolling = true;
	
	event.stopPropagation();
	event.preventDefault();
}

Scrollbar.prototype._mouseoutTrack = function(event)
{
	this._track_scrolling = false;
	
	event.stopPropagation();
	event.preventDefault();
}

Scrollbar.prototype._mouseupTrack = function(event)
{
	clearInterval(this._track_timer);
	
	// clear temporary event handlers
	this._track.removeEventListener("mousemove", this._mousemoveTrackHandler, true);
	this._track.removeEventListener("mouseover", this._mouseoverTrackHandler, true);
	this._track.removeEventListener("mouseout", this._mouseoutTrackHandler, true);
	document.removeEventListener("mouseup", this._mouseupTrackHandler, true);
	
	// remove temporary properties
	delete this._track_mouse_temp;
	delete this._track_scrolling;
	delete this._track_timer;
	
	event.stopPropagation();
	event.preventDefault();
}

Scrollbar.prototype._trackScrollOnePage = function(self)
{
	// this is called from setInterval, so we need a ptr to this
	
	if (!self._track_scrolling) return;
	
	var deltaScroll = Math.round(self._trackLength * self._getViewToContentRatio());
	
	if (self._track_mouse_temp < self._thumbStart)
		self.scrollByThumbDelta(-deltaScroll);
	else if (self._track_mouse_temp > (self._thumbStart + self._thumbLength))
		self.scrollByThumbDelta(deltaScroll);
}

/*
 * setScrollArea(scrollarea)
 * Sets the ScrollArea this scrollbar is using.
 */
Scrollbar.prototype.setScrollArea = function(scrollarea)
{
	// if we already have a scrollarea, remove the mousewheel handler
	if (this.scrollarea)
	{
		this.scrollbar.removeEventListener("mousewheel", this.scrollarea._mousewheelScrollHandler, true);
	}
	
	this.scrollarea = scrollarea;
	
	// add mousewheel handler
	this.scrollbar.addEventListener("mousewheel", this.scrollarea._mousewheelScrollHandler, true);
}

/*
 * refresh()
 * Refresh the scrollbar size and thumb position
 */
Scrollbar.prototype.refresh = function()
{
	this._trackOffset = this._computeTrackOffset();
	this._trackLength = this._computeTrackLength();
	
	var ratio = this._getViewToContentRatio();
	
	if (ratio >= 1.0 || !this._canScroll())
	{
		if (this.autohide)
		{
			// hide the scrollbar, all content is visible
			this.hide();
		}
		
		// hide the thumb
		this._thumb.style.display = "none";
		this.scrollbar.style.appleDashboardRegion = "none";
	}
	else
	{
		this._thumbLength = Math.max(Math.round(this._trackLength * ratio), this.minThumbSize);
		this._numScrollablePixels = this._trackLength - this._thumbLength - (2 * this.padding);
		
		this._setObjectLength(this._thumb, this._thumbLength);
				
		// show the thumb
		this._thumb.style.display = "block";
		this.scrollbar.style.appleDashboardRegion = "dashboard-region(control rectangle)";
		
		this.show();
	}
	
	// Make sure position is updated appropriately
	this.verticalHasScrolled();
	this.horizontalHasScrolled();
}

Scrollbar.prototype.setAutohide = function(autohide)
{
	this.autohide = autohide;
	
	// hide the scrollbar if necessary
	if (this._getViewToContentRatio() >= 1.0 && autohide)
	{
		this.hide();
	}
	else
	{
		this.show();
	}
}

Scrollbar.prototype.hide = function()
{
	this._track.style.display = "none";
	this.scrollbar.style.display = "none";
	this.hidden = true;
}

Scrollbar.prototype.show = function()
{
	this._track.style.display = "block";
	this.scrollbar.style.removeProperty("display");
	if (this.hidden) {
		this.hidden = false;
		this.refresh();
	}
}

Scrollbar.prototype.setSize = function(size)
{
	this.size = size;
	
	this._setObjectSize(this.scrollbar, size);
	this._setObjectSize(this._track.children[1], size);
	this._setObjectSize(this._thumb.children[1], size);
}

Scrollbar.prototype.setTrackStart = function(imgpath, length)
{
	this.trackStartPath = imgpath;
	this.trackStartLength = length;

	var element = this._track.children[0];
	element.style.background = "url(" + imgpath + ") no-repeat top left";
	this._setObjectLength(element, length);
	this._setObjectSize(element, this.size);
	this._setObjectStart(this._track.children[1], length);
}

Scrollbar.prototype.setTrackMiddle = function(imgpath)
{
	this.trackMiddlePath = imgpath;

	this._track.children[1].style.background = "url(" + imgpath + ") " + this._repeatType + " top left";
}

Scrollbar.prototype.setTrackEnd = function(imgpath, length)
{
	this.trackEndPath = imgpath;
	this.trackEndLength = length;

	var element = this._track.children[2];
	element.style.background = "url(" + imgpath + ") no-repeat top left";
	this._setObjectLength(element, length);
	this._setObjectSize(element, this.size);
	this._setObjectEnd(this._track.children[1], length);
}

Scrollbar.prototype.setThumbStart = function(imgpath, length)
{
	this.thumbStartPath = imgpath;
	this.thumbStartLength = length;
	
	var element = this._thumb.children[0];
	element.style.background = "url(" + imgpath + ") no-repeat top left";
	this._setObjectLength(element, length);
	this._setObjectSize(element, this.size);
	this._setObjectStart(this._thumb.children[1], length);
}

Scrollbar.prototype.setThumbMiddle = function(imgpath)
{
	this.thumbMiddlePath = imgpath;
	
	this._thumb.children[1].style.background = "url(" + imgpath + ") " + this._repeatType + " top left";
}

Scrollbar.prototype.setThumbEnd = function(imgpath, length)
{
	this.thumbEndPath = imgpath;
	this.thumbEndLength = length;

	var element = this._thumb.children[2];
	element.style.background = "url(" + imgpath + ") no-repeat top left";
	this._setObjectLength(element, length);
	this._setObjectSize(element, this.size);
	this._setObjectEnd(this._thumb.children[1], length);
}

Scrollbar.prototype._contentPositionForThumbPosition = function(thumb_pos)
{
	// if we're currently displaying all content, we don't want it outside the view
	if (this._getViewToContentRatio() >= 1.0)
	{
		return 0;
	}
	else
	{
		return (thumb_pos - this.padding) * ((this._getContentLength() - this._getViewLength()) / this._numScrollablePixels);
	}
}

Scrollbar.prototype._thumbPositionForContentPosition = function(page_pos)
{
	// if we're currently displaying all content, we don't want it outside the view
	if (this._getViewToContentRatio() >= 1.0)
	{
		return this.padding;
	}
	else
	{
		var result = this.padding - -(page_pos / ((this._getContentLength() - this._getViewLength()) / this._numScrollablePixels));
		if (isNaN(result))
			result = 0;
		return result;
	}
}

Scrollbar.prototype.scrollByThumbDelta = function(deltaScroll)
{
	if (!deltaScroll)
		return;
	
	this.scrollTo(this._contentPositionForThumbPosition(this._thumbStart + deltaScroll));
}


/*******************************************************************************
 * VerticalScrollbar
 * Implementation of Scrollbar
 *
 *
 */

function VerticalScrollbar(scrollbar)
{
	/* Objects */
	this.scrollarea = null;
	this.scrollbar = scrollbar;
	
	/* public properties */
	// These are read-write. Set them as needed.
	this.minThumbSize = 28;
	this.padding = -1;
	
	// These are read-only. Use the setter functions to set them.
	this.autohide = true;
	this.hidden = true;
	this.size = 19; // width
	this.trackStartPath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_track_vtop.png";
	this.trackStartLength = 18; // height
	this.trackMiddlePath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_track_vmid.png";
	this.trackEndPath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_track_vbottom.png";
	this.trackEndLength = 18; // height
	this.thumbStartPath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_thumb_vtop.png";
	this.thumbStartLength = 9; // height
	this.thumbMiddlePath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_thumb_vmid.png";
	this.thumbEndPath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_thumb_vbottom.png";
	this.thumbEndLength = 9; // height

	/* Internal objects */
	this._track = null;
	this._thumb = null;
	
	/* Dimensions */
	// these only need to be set during refresh()
	this._trackOffset = 0;
	this._trackLength = 0;
	this._numScrollablePixels = 0;
	this._thumbLength = 0;
	this._repeatType = "repeat-y";
	
	// these change as the content is scrolled
	this._thumbStart = this.padding;
	
	// For JavaScript event handlers
	var _self = this;
	
	this._captureEventHandler = function(event) { _self._captureEvent(event); };
	this._mousedownThumbHandler = function(event) { _self._mousedownThumb(event); };
	this._mousemoveThumbHandler = function(event) { _self._mousemoveThumb(event); };
	this._mouseupThumbHandler = function(event) { _self._mouseupThumb(event); };
	this._mousedownTrackHandler = function(event) { _self._mousedownTrack(event); };
	this._mousemoveTrackHandler = function(event) { _self._mousemoveTrack(event); };
	this._mouseoverTrackHandler = function(event) { _self._mouseoverTrack(event); };
	this._mouseoutTrackHandler = function(event) { _self._mouseoutTrack(event); };
	this._mouseupTrackHandler = function(event) { _self._mouseupTrack(event); };
	
	this._init();
}

// Inherit from Scrollbar
VerticalScrollbar.prototype = new Scrollbar(null);


/*********************
 * Orientation-specific functions.
 * These helper functions return vertical values.
 */
VerticalScrollbar.prototype.scrollTo = function(pos)
{
	this.scrollarea.verticalScrollTo(pos);
}

VerticalScrollbar.prototype._setObjectSize = function(object, size)
{ object.style.width = size + "px"; }

VerticalScrollbar.prototype._setObjectLength = function(object, length)
{
	if (!isNaN(length))
		object.style.height = length + "px";
}

VerticalScrollbar.prototype._setObjectStart = function(object, start)
{ object.style.top = start + "px"; }

VerticalScrollbar.prototype._setObjectEnd = function(object, end)
{ object.style.bottom = end + "px"; }

VerticalScrollbar.prototype._getMousePosition = function(event)
{
	if (event)
		return event.y;
	else
		return 0;
}
	
VerticalScrollbar.prototype._getThumbStartPos = function()
{
	return parseInt(document.defaultView.getComputedStyle(this._thumb, '').getPropertyValue("top"), 10);
}

VerticalScrollbar.prototype._computeTrackOffset = function()
{
	// get the absolute top of the track
	var obj = this.scrollbar;
	var curtop = 0;
	while (obj.offsetParent)
	{
		curtop += obj.offsetTop;
		obj = obj.offsetParent;
	}
	
	return curtop;
}

VerticalScrollbar.prototype._computeTrackLength = function()
{
	// get the current actual track height
	var style = document.defaultView.getComputedStyle(this.scrollbar, '');
	return style ? parseInt(style.getPropertyValue("height"), 10) : 0;
}

VerticalScrollbar.prototype._getViewToContentRatio = function()
{ return this.scrollarea.viewToContentHeightRatio; }

VerticalScrollbar.prototype._getContentLength = function()
{ return this.scrollarea.content.scrollHeight; }

VerticalScrollbar.prototype._getViewLength = function()
{ return this.scrollarea.viewHeight; }

VerticalScrollbar.prototype._canScroll = function()
{ return this.scrollarea.scrollsVertically; }


VerticalScrollbar.prototype.verticalHasScrolled = function()
{
	var new_thumb_pos = this._thumbPositionForContentPosition(this.scrollarea.content.scrollTop);
	this._thumbStart = new_thumb_pos;
	this._thumb.style.top = new_thumb_pos + "px";
}

VerticalScrollbar.prototype.horizontalHasScrolled = function()
{
}



/*******************************************************************************
* HorizontalScrollbar
* Implementation of Scrollbar
*
*
*/

function HorizontalScrollbar(scrollbar)
{
	/* Objects */
	this.scrollarea = null;
	this.scrollbar = scrollbar;
	
	/* public properties */
	// These are read-write. Set them as needed.
	this.minThumbSize = 28;
	this.padding = -1;
	
	// These are read-only. Use the setter functions to set them.
	this.autohide = true;
	this.hidden = true;
	this.size = 19; // height
	this.trackStartPath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_track_hleft.png";
	this.trackStartLength = 18; // width
	this.trackMiddlePath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_track_hmid.png";
	this.trackEndPath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_track_hright.png";
	this.trackEndLength = 18; // width
	this.thumbStartPath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_thumb_hleft.png";
	this.thumbStartLength = 9; // width
	this.thumbMiddlePath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_thumb_hmid.png";
	this.thumbEndPath = "file:///System/Library/WidgetResources/AppleClasses/Images/scroll_thumb_hright.png";
	this.thumbEndLength = 9; // width
	
	/* Internal objects */
	this._track = null;
	this._thumb = null;
	
	/* Dimensions */
	// these only need to be set during refresh()
	this._trackOffset = 0;
	this._trackLength = 0;
	this._numScrollablePixels = 0;
	this._thumbLength = 0;
	this._repeatType = "repeat-x";
	
	// these change as the content is scrolled
	this._thumbStart = this.padding;
	
	// For JavaScript event handlers
	var _self = this;
	
	this._captureEventHandler = function(event) { _self._captureEvent(event); };
	this._mousedownThumbHandler = function(event) { _self._mousedownThumb(event); };
	this._mousemoveThumbHandler = function(event) { _self._mousemoveThumb(event); };
	this._mouseupThumbHandler = function(event) { _self._mouseupThumb(event); };
	this._mousedownTrackHandler = function(event) { _self._mousedownTrack(event); };
	this._mousemoveTrackHandler = function(event) { _self._mousemoveTrack(event); };
	this._mouseoverTrackHandler = function(event) { _self._mouseoverTrack(event); };
	this._mouseoutTrackHandler = function(event) { _self._mouseoutTrack(event); };
	this._mouseupTrackHandler = function(event) { _self._mouseupTrack(event); };
	
	this._init();
}

// Inherit from Scrollbar
HorizontalScrollbar.prototype = new Scrollbar(null);


/*********************
* Orientation-specific functions.
* These helper functions return vertical values.
*/
HorizontalScrollbar.prototype.scrollTo = function(pos)
{
	this.scrollarea.horizontalScrollTo(pos);
}

HorizontalScrollbar.prototype._setObjectSize = function(object, size)
{ object.style.height = size + "px"; }

HorizontalScrollbar.prototype._setObjectLength = function(object, length)
{
	if (!isNaN(length))
		object.style.height = length + "px";
}

HorizontalScrollbar.prototype._setObjectStart = function(object, start)
{ object.style.left = start + "px"; }

HorizontalScrollbar.prototype._setObjectEnd = function(object, end)
{ object.style.right = end + "px"; }

HorizontalScrollbar.prototype._getMousePosition = function(event)
{
	if (event)
		return event.x;
	else
		return 0;
}

HorizontalScrollbar.prototype._getThumbStartPos = function()
{
	return parseInt(document.defaultView.getComputedStyle(this._thumb, '').getPropertyValue("left"), 10);
}

HorizontalScrollbar.prototype._computeTrackOffset = function()
{
	// get the absolute top of the track
	var obj = this.scrollbar;
	var curtop = 0;
	while (obj.offsetParent)
	{
		curtop += obj.offsetLeft;
		obj = obj.offsetParent;
	}
	
	return curtop;
}

HorizontalScrollbar.prototype._computeTrackLength = function()
{
	// get the current actual track height
	var style = document.defaultView.getComputedStyle(this.scrollbar, '');
	return style ? parseInt(style.getPropertyValue("width"), 10) : 0;
}

HorizontalScrollbar.prototype._getViewToContentRatio = function()
{ return this.scrollarea.viewToContentWidthRatio; }

HorizontalScrollbar.prototype._getContentLength = function()
{ return this.scrollarea.content.scrollWidth; }

HorizontalScrollbar.prototype._getViewLength = function()
{ return this.scrollarea.viewWidth; }

HorizontalScrollbar.prototype._canScroll = function()
{ return this.scrollarea.scrollsHorizontally; }


HorizontalScrollbar.prototype.verticalHasScrolled = function()
{
}

HorizontalScrollbar.prototype.horizontalHasScrolled = function()
{
	var new_thumb_pos = this._thumbPositionForContentPosition(this.scrollarea.content.scrollLeft);
	this._thumbStart = new_thumb_pos;
	this._thumb.style.left = new_thumb_pos + "px";
}
