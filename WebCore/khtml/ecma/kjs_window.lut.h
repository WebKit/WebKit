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

}; // namespace

namespace KJS {

const struct HashEntry WindowTableEntries[] = {
   { "close", Window::Close, DontDelete|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "alert", Window::Alert, DontDelete|Function, 1, 0 },
   { "outerHeight", Window::OuterHeight, DontDelete|ReadOnly, 0, &WindowTableEntries[92] },
   { "ondragdrop", Window::Ondragdrop, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "offscreenBuffering", Window::OffscreenBuffering, DontDelete|ReadOnly, 0, 0 },
   { "setTimeout", Window::SetTimeout, DontDelete|Function, 2, 0 },
   { "getSelection", Window::GetSelection, DontDelete|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "DOMException", Window::DOMException, DontDelete, 0, &WindowTableEntries[103] },
   { "closed", Window::Closed, DontDelete|ReadOnly, 0, &WindowTableEntries[96] },
   { "event", Window::Event, DontDelete|ReadOnly, 0, &WindowTableEntries[108] },
   { 0, 0, 0, 0, 0 },
   { "onload", Window::Onload, DontDelete, 0, 0 },
   { "frames", Window::Frames, DontDelete|ReadOnly, 0, 0 },
   { "screenX", Window::ScreenX, DontDelete|ReadOnly, 0, &WindowTableEntries[105] },
   { "clientInformation", Window::ClientInformation, DontDelete|ReadOnly, 0, &WindowTableEntries[94] },
   { "CSSRule", Window::CSSRule, DontDelete, 0, 0 },
   { "length", Window::Length, DontDelete|ReadOnly, 0, &WindowTableEntries[91] },
   { "personalbar", Window::Personalbar, DontDelete|ReadOnly, 0, 0 },
   { "clearInterval", Window::ClearInterval, DontDelete|Function, 1, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onmouseup", Window::Onmouseup, DontDelete, 0, 0 },
   { "NodeFilter", Window::NodeFilter, DontDelete, 0, 0 },
   { "opener", Window::Opener, DontDelete|ReadOnly, 0, 0 },
   { "parent", Window::Parent, DontDelete|ReadOnly, 0, &WindowTableEntries[113] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "scrollX", Window::ScrollX, DontDelete|ReadOnly, 0, &WindowTableEntries[110] },
   { "scroll", Window::Scroll, DontDelete|Function, 2, &WindowTableEntries[98] },
   { 0, 0, 0, 0, 0 },
   { "Node", Window::Node, DontDelete, 0, &WindowTableEntries[112] },
   { "onblur", Window::Onblur, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onmove", Window::Onmove, DontDelete, 0, 0 },
   { "Image", Window::Image, DontDelete|ReadOnly, 0, &WindowTableEntries[104] },
   { "onkeypress", Window::Onkeypress, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "scrollBy", Window::ScrollBy, DontDelete|Function, 2, &WindowTableEntries[101] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "resizeBy", Window::ResizeBy, DontDelete|Function, 2, 0 },
   { "onabort", Window::Onabort, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "addEventListener", Window::AddEventListener, DontDelete|Function, 3, 0 },
   { "Range", Window::Range, DontDelete, 0, &WindowTableEntries[93] },
   { "pageYOffset", Window::PageYOffset, DontDelete|ReadOnly, 0, &WindowTableEntries[97] },
   { "crypto", Window::Crypto, DontDelete|ReadOnly, 0, &WindowTableEntries[89] },
   { "prompt", Window::Prompt, DontDelete|Function, 2, 0 },
   { "resizeTo", Window::ResizeTo, DontDelete|Function, 2, 0 },
   { "status", Window::Status, DontDelete, 0, &WindowTableEntries[114] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "location", Window::_Location, DontDelete, 0, &WindowTableEntries[95] },
   { "screenTop", Window::ScreenTop, DontDelete|ReadOnly, 0, &WindowTableEntries[116] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onselect", Window::Onselect, DontDelete, 0, 0 },
   { "name", Window::Name, DontDelete, 0, &WindowTableEntries[119] },
   { "document", Window::Document, DontDelete|ReadOnly, 0, 0 },
   { "onerror", Window::Onerror, DontDelete, 0, &WindowTableEntries[122] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "ondblclick", Window::Ondblclick, DontDelete, 0, 0 },
   { "onkeyup", Window::Onkeyup, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "Event", Window::EventCtor, DontDelete, 0, 0 },
   { "self", Window::Self, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "top", Window::Top, DontDelete|ReadOnly, 0, 0 },
   { "innerHeight", Window::InnerHeight, DontDelete|ReadOnly, 0, &WindowTableEntries[90] },
   { "history", Window::_History, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "open", Window::Open, DontDelete|Function, 3, &WindowTableEntries[121] },
   { 0, 0, 0, 0, 0 },
   { "onsubmit", Window::Onsubmit, DontDelete, 0, 0 },
   { "navigator", Window::_Navigator, DontDelete|ReadOnly, 0, &WindowTableEntries[107] },
   { "defaultstatus", Window::DefaultStatus, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "defaultStatus", Window::DefaultStatus, DontDelete, 0, &WindowTableEntries[109] },
   { "innerWidth", Window::InnerWidth, DontDelete|ReadOnly, 0, 0 },
   { "konqueror", Window::_Konqueror, DontDelete|ReadOnly, 0, 0 },
   { "outerWidth", Window::OuterWidth, DontDelete|ReadOnly, 0, &WindowTableEntries[99] },
   { "pageXOffset", Window::PageXOffset, DontDelete|ReadOnly, 0, &WindowTableEntries[111] },
   { "screenY", Window::ScreenY, DontDelete|ReadOnly, 0, &WindowTableEntries[102] },
   { "screenLeft", Window::ScreenLeft, DontDelete|ReadOnly, 0, &WindowTableEntries[117] },
   { "scrollbars", Window::Scrollbars, DontDelete|ReadOnly, 0, &WindowTableEntries[100] },
   { "scrollTo", Window::ScrollTo, DontDelete|Function, 2, 0 },
   { "scrollY", Window::ScrollY, DontDelete|ReadOnly, 0, 0 },
   { "moveBy", Window::MoveBy, DontDelete|Function, 2, 0 },
   { "moveTo", Window::MoveTo, DontDelete|Function, 2, &WindowTableEntries[115] },
   { "window", Window::_Window, DontDelete|ReadOnly, 0, 0 },
   { "screen", Window::_Screen, DontDelete|ReadOnly, 0, 0 },
   { "Option", Window::Option, DontDelete|ReadOnly, 0, &WindowTableEntries[106] },
   { "confirm", Window::Confirm, DontDelete|Function, 1, 0 },
   { "clearTimeout", Window::ClearTimeout, DontDelete|Function, 1, 0 },
   { "focus", Window::Focus, DontDelete|Function, 0, 0 },
   { "blur", Window::Blur, DontDelete|Function, 0, 0 },
   { "setInterval", Window::SetInterval, DontDelete|Function, 2, 0 },
   { "captureEvents", Window::CaptureEvents, DontDelete|Function, 0, &WindowTableEntries[118] },
   { "releaseEvents", Window::ReleaseEvents, DontDelete|Function, 0, 0 },
   { "removeEventListener", Window::RemoveEventListener, DontDelete|Function, 3, 0 },
   { "onchange", Window::Onchange, DontDelete, 0, 0 },
   { "onclick", Window::Onclick, DontDelete, 0, 0 },
   { "onfocus", Window::Onfocus, DontDelete, 0, 0 },
   { "onkeydown", Window::Onkeydown, DontDelete, 0, 0 },
   { "onmousedown", Window::Onmousedown, DontDelete, 0, 0 },
   { "onmousemove", Window::Onmousemove, DontDelete, 0, &WindowTableEntries[120] },
   { "onmouseout", Window::Onmouseout, DontDelete, 0, 0 },
   { "onmouseover", Window::Onmouseover, DontDelete, 0, 0 },
   { "onreset", Window::Onreset, DontDelete, 0, 0 },
   { "onresize", Window::Onresize, DontDelete, 0, 0 },
   { "onunload", Window::Onunload, DontDelete, 0, 0 }
};

const struct HashTable WindowTable = { 2, 123, WindowTableEntries, 89 };

}; // namespace

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

}; // namespace

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

}; // namespace
