/* Automatically generated from kjs_window.cpp using ../../../JavaScriptCore/kjs/create_hash_table. DO NOT EDIT ! */

namespace KJS {

const struct HashEntry ScreenTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "colorDepth", Screen::ColorDepth, DontEnum|ReadOnly, 0, &ScreenTableEntries[9] },
   { 0, 0, 0, 0, 0 },
   { "height", Screen::Height, DontEnum|ReadOnly, 0, &ScreenTableEntries[7] },
   { "pixelDepth", Screen::PixelDepth, DontEnum|ReadOnly, 0, 0 },
   { "width", Screen::Width, DontEnum|ReadOnly, 0, 0 },
   { "availTop", Screen::AvailTop, DontEnum|ReadOnly, 0, &ScreenTableEntries[8] },
   { "availLeft", Screen::AvailLeft, DontEnum|ReadOnly, 0, 0 },
   { "availHeight", Screen::AvailHeight, DontEnum|ReadOnly, 0, 0 },
   { "availWidth", Screen::AvailWidth, DontEnum|ReadOnly, 0, 0 }
};

const struct HashTable ScreenTable = { 2, 10, ScreenTableEntries, 7 };

} // namespace

namespace KJS {

const struct HashEntry WindowTableEntries[] = {
   { "event", Window::Event, DontDelete, 0, &WindowTableEntries[94] },
   { "frames", Window::Frames, DontDelete|ReadOnly, 0, &WindowTableEntries[95] },
   { "onmouseup", Window::Onmouseup, DontDelete, 0, 0 },
   { "NodeFilter", Window::NodeFilter, DontDelete, 0, &WindowTableEntries[102] },
   { "CSSRule", Window::CSSRule, DontDelete, 0, &WindowTableEntries[111] },
   { "length", Window::Length, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "print", Window::Print, DontDelete|Function, 2, &WindowTableEntries[110] },
   { "opener", Window::Opener, DontDelete|ReadOnly, 0, 0 },
   { "parent", Window::Parent, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "scrollX", Window::ScrollX, DontDelete|ReadOnly, 0, &WindowTableEntries[118] },
   { "scrollY", Window::ScrollY, DontDelete|ReadOnly, 0, &WindowTableEntries[112] },
   { "XMLSerializer", Window::XMLSerializer, DontDelete|ReadOnly, 0, 0 },
   { "menubar", Window::Menubar, DontDelete|ReadOnly, 0, &WindowTableEntries[97] },
   { 0, 0, 0, 0, 0 },
   { "defaultStatus", Window::DefaultStatus, DontDelete, 0, &WindowTableEntries[108] },
   { "onblur", Window::Onblur, DontDelete, 0, 0 },
   { "confirm", Window::Confirm, DontDelete|Function, 1, 0 },
   { "scrollBy", Window::ScrollBy, DontDelete|Function, 2, &WindowTableEntries[123] },
   { "pageXOffset", Window::PageXOffset, DontDelete|ReadOnly, 0, 0 },
   { "pageYOffset", Window::PageYOffset, DontDelete|ReadOnly, 0, 0 },
   { "Node", Window::Node, DontDelete, 0, &WindowTableEntries[99] },
   { "toolbar", Window::Toolbar, DontDelete|ReadOnly, 0, &WindowTableEntries[101] },
   { "Image", Window::Image, DontDelete|ReadOnly, 0, 0 },
   { "onabort", Window::Onabort, DontDelete, 0, 0 },
   { "onmousemove", Window::Onmousemove, DontDelete, 0, 0 },
   { "scrollTo", Window::ScrollTo, DontDelete|Function, 2, &WindowTableEntries[121] },
   { "onsearch", Window::Onsearch, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "screenLeft", Window::ScreenLeft, DontDelete|ReadOnly, 0, &WindowTableEntries[100] },
   { "onmouseover", Window::Onmouseover, DontDelete, 0, 0 },
   { "crypto", Window::Crypto, DontDelete|ReadOnly, 0, 0 },
   { "screenTop", Window::ScreenTop, DontDelete|ReadOnly, 0, &WindowTableEntries[103] },
   { "Range", Window::Range, DontDelete, 0, &WindowTableEntries[91] },
   { "status", Window::Status, DontDelete, 0, 0 },
   { "onreset", Window::Onreset, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onselect", Window::Onselect, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "document", Window::Document, DontDelete|ReadOnly, 0, &WindowTableEntries[114] },
   { "onunload", Window::Onunload, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onerror", Window::Onerror, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "innerHeight", Window::InnerHeight, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "innerWidth", Window::InnerWidth, DontDelete|ReadOnly, 0, &WindowTableEntries[119] },
   { "defaultstatus", Window::DefaultStatus, DontDelete, 0, 0 },
   { "name", Window::Name, DontDelete, 0, 0 },
   { "frameElement", Window::FrameElement, DontDelete|ReadOnly, 0, 0 },
   { "offscreenBuffering", Window::OffscreenBuffering, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onscroll", Window::Onscroll, DontDelete, 0, 0 },
   { "history", Window::_History, DontDelete|ReadOnly, 0, 0 },
   { "Event", Window::EventCtor, DontDelete, 0, 0 },
   { "onresize", Window::Onresize, DontDelete, 0, 0 },
   { "navigator", Window::_Navigator, DontDelete|ReadOnly, 0, 0 },
   { "self", Window::Self, DontDelete|ReadOnly, 0, &WindowTableEntries[124] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "top", Window::Top, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "clientInformation", Window::ClientInformation, DontDelete|ReadOnly, 0, &WindowTableEntries[93] },
   { 0, 0, 0, 0, 0 },
   { "outerWidth", Window::OuterWidth, DontDelete|ReadOnly, 0, &WindowTableEntries[105] },
   { "getSelection", Window::GetSelection, DontDelete|Function, 0, &WindowTableEntries[115] },
   { 0, 0, 0, 0, 0 },
   { "blur", Window::Blur, DontDelete|Function, 0, 0 },
   { "locationbar", Window::Locationbar, DontDelete, 0, &WindowTableEntries[106] },
   { "DOMException", Window::DOMException, DontDelete, 0, &WindowTableEntries[96] },
   { 0, 0, 0, 0, 0 },
   { "setInterval", Window::SetInterval, DontDelete|Function, 2, 0 },
   { "scrollbars", Window::Scrollbars, DontDelete|ReadOnly, 0, 0 },
   { "clearTimeout", Window::ClearTimeout, DontDelete|Function, 1, &WindowTableEntries[107] },
   { "moveBy", Window::MoveBy, DontDelete|Function, 2, &WindowTableEntries[117] },
   { "alert", Window::Alert, DontDelete|Function, 1, 0 },
   { "clearInterval", Window::ClearInterval, DontDelete|Function, 1, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "personalbar", Window::Personalbar, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "Option", Window::Option, DontDelete|ReadOnly, 0, 0 },
   { "closed", Window::Closed, DontDelete|ReadOnly, 0, &WindowTableEntries[92] },
   { "focus", Window::Focus, DontDelete|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "location", Window::_Location, DontDelete, 0, 0 },
   { "konqueror", Window::_Konqueror, DontDelete|ReadOnly, 0, &WindowTableEntries[98] },
   { "outerHeight", Window::OuterHeight, DontDelete|ReadOnly, 0, 0 },
   { "screenX", Window::ScreenX, DontDelete|ReadOnly, 0, &WindowTableEntries[120] },
   { "screenY", Window::ScreenY, DontDelete|ReadOnly, 0, &WindowTableEntries[109] },
   { "statusbar", Window::Statusbar, DontDelete|ReadOnly, 0, 0 },
   { "scroll", Window::Scroll, DontDelete|Function, 2, 0 },
   { "moveTo", Window::MoveTo, DontDelete|Function, 2, 0 },
   { "resizeBy", Window::ResizeBy, DontDelete|Function, 2, &WindowTableEntries[122] },
   { "resizeTo", Window::ResizeTo, DontDelete|Function, 2, 0 },
   { "window", Window::_Window, DontDelete|ReadOnly, 0, 0 },
   { "screen", Window::_Screen, DontDelete|ReadOnly, 0, 0 },
   { "XMLHttpRequest", Window::XMLHttpRequest, DontDelete|ReadOnly, 0, &WindowTableEntries[104] },
   { "prompt", Window::Prompt, DontDelete|Function, 2, &WindowTableEntries[116] },
   { "open", Window::Open, DontDelete|Function, 3, 0 },
   { "setTimeout", Window::SetTimeout, DontDelete|Function, 2, 0 },
   { "close", Window::Close, DontDelete|Function, 0, 0 },
   { "captureEvents", Window::CaptureEvents, DontDelete|Function, 0, 0 },
   { "releaseEvents", Window::ReleaseEvents, DontDelete|Function, 0, 0 },
   { "addEventListener", Window::AddEventListener, DontDelete|Function, 3, &WindowTableEntries[113] },
   { "removeEventListener", Window::RemoveEventListener, DontDelete|Function, 3, 0 },
   { "onchange", Window::Onchange, DontDelete, 0, 0 },
   { "onclick", Window::Onclick, DontDelete, 0, 0 },
   { "ondblclick", Window::Ondblclick, DontDelete, 0, 0 },
   { "ondragdrop", Window::Ondragdrop, DontDelete, 0, 0 },
   { "onfocus", Window::Onfocus, DontDelete, 0, 0 },
   { "onkeydown", Window::Onkeydown, DontDelete, 0, 0 },
   { "onkeypress", Window::Onkeypress, DontDelete, 0, 0 },
   { "onkeyup", Window::Onkeyup, DontDelete, 0, 0 },
   { "onload", Window::Onload, DontDelete, 0, 0 },
   { "onmousedown", Window::Onmousedown, DontDelete, 0, 0 },
   { "onmouseout", Window::Onmouseout, DontDelete, 0, 0 },
   { "onmove", Window::Onmove, DontDelete, 0, 0 },
   { "onsubmit", Window::Onsubmit, DontDelete, 0, 0 }
};

const struct HashTable WindowTable = { 2, 125, WindowTableEntries, 91 };

} // namespace

namespace KJS {

const struct HashEntry LocationTableEntries[] = {
   { "toString", Location::ToString, DontDelete|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "hash", Location::Hash, DontDelete, 0, &LocationTableEntries[11] },
   { "href", Location::Href, DontDelete, 0, &LocationTableEntries[13] },
   { "reload", Location::Reload, DontDelete|Function, 0, 0 },
   { "hostname", Location::Hostname, DontDelete, 0, 0 },
   { "host", Location::Host, DontDelete, 0, &LocationTableEntries[14] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "pathname", Location::Pathname, DontDelete, 0, 0 },
   { "port", Location::Port, DontDelete, 0, &LocationTableEntries[12] },
   { "protocol", Location::Protocol, DontDelete, 0, 0 },
   { "search", Location::Search, DontDelete, 0, 0 },
   { "[[==]]", Location::EqualEqual, DontDelete|ReadOnly, 0, &LocationTableEntries[15] },
   { "replace", Location::Replace, DontDelete|Function, 1, 0 }
};

const struct HashTable LocationTable = { 2, 16, LocationTableEntries, 11 };

} // namespace

namespace KJS {

const struct HashEntry SelectionTableEntries[] = {
   { "focusOffset", Selection::FocusOffset, DontDelete|ReadOnly, 0, &SelectionTableEntries[20] },
   { 0, 0, 0, 0, 0 },
   { "modify", Selection::Modify, DontDelete|Function, 3, 0 },
   { "focusNode", Selection::FocusNode, DontDelete|ReadOnly, 0, &SelectionTableEntries[19] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "extentOffset", Selection::FocusOffset, DontDelete|ReadOnly, 0, 0 },
   { "setPosition", Selection::SetPosition, DontDelete|Function, 2, 0 },
   { "empty", Selection::Empty, DontDelete|Function, 0, 0 },
   { "extentNode", Selection::FocusNode, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "type", Selection::_Type, DontDelete|ReadOnly, 0, 0 },
   { "collapseToEnd", Selection::CollapseToEnd, DontDelete|Function, 0, &SelectionTableEntries[25] },
   { "anchorOffset", Selection::AnchorOffset, DontDelete|ReadOnly, 0, &SelectionTableEntries[22] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "anchorNode", Selection::AnchorNode, DontDelete|ReadOnly, 0, &SelectionTableEntries[21] },
   { "baseNode", Selection::AnchorNode, DontDelete|ReadOnly, 0, &SelectionTableEntries[23] },
   { "baseOffset", Selection::AnchorOffset, DontDelete|ReadOnly, 0, 0 },
   { "isCollapsed", Selection::IsCollapsed, DontDelete|ReadOnly, 0, 0 },
   { "[[==]]", Selection::EqualEqual, DontDelete|ReadOnly, 0, &SelectionTableEntries[24] },
   { "toString", Selection::ToString, DontDelete|Function, 0, &SelectionTableEntries[26] },
   { "collapse", Selection::Collapse, DontDelete|Function, 2, 0 },
   { "collapseToStart", Selection::CollapseToStart, DontDelete|Function, 0, 0 },
   { "setBaseAndExtent", Selection::SetBaseAndExtent, DontDelete|Function, 4, 0 }
};

const struct HashTable SelectionTable = { 2, 27, SelectionTableEntries, 19 };

} // namespace

namespace KJS {

const struct HashEntry BarInfoTableEntries[] = {
   { "visible", BarInfo::Visible, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable BarInfoTable = { 2, 1, BarInfoTableEntries, 1 };

} // namespace

namespace KJS {

const struct HashEntry HistoryTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "back", History::Back, DontDelete|Function, 0, &HistoryTableEntries[4] },
   { "length", History::Length, DontDelete|ReadOnly, 0, &HistoryTableEntries[5] },
   { 0, 0, 0, 0, 0 },
   { "forward", History::Forward, DontDelete|Function, 0, 0 },
   { "go", History::Go, DontDelete|Function, 1, 0 }
};

const struct HashTable HistoryTable = { 2, 6, HistoryTableEntries, 4 };

} // namespace
