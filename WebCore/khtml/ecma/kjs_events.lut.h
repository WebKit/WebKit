/* Automatically generated from kjs_events.cpp using ../../../JavaScriptCore/kjs/create_hash_table. DO NOT EDIT ! */

namespace KJS {

const struct HashEntry EventConstructorTableEntries[] = {
   { "CAPTURING_PHASE", DOM::Event::CAPTURING_PHASE, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[3] },
   { "BUBBLING_PHASE", DOM::Event::BUBBLING_PHASE, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[6] },
   { "MOUSEOUT", 8, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[7] },
   { "AT_TARGET", DOM::Event::AT_TARGET, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[4] },
   { "MOUSEDOWN", 1, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[5] },
   { "MOUSEUP", 2, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[13] },
   { "MOUSEOVER", 4, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[8] },
   { "MOUSEMOVE", 16, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[11] },
   { "MOUSEDRAG", 32, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[9] },
   { "CLICK", 64, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[10] },
   { "DBLCLICK", 128, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[14] },
   { "KEYDOWN", 256, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[12] },
   { "KEYUP", 512, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[18] },
   { "KEYPRESS", 1024, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[15] },
   { "DRAGDROP", 2048, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[17] },
   { "FOCUS", 4096, DontDelete|ReadOnly, 0, &EventConstructorTableEntries[16] },
   { "BLUR", 8192, DontDelete|ReadOnly, 0, 0 },
   { "SELECT", 16384, DontDelete|ReadOnly, 0, 0 },
   { "CHANGE", 32768, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable EventConstructorTable = { 2, 19, EventConstructorTableEntries, 3 };

} // namespace

namespace KJS {

const struct HashEntry DOMEventTableEntries[] = {
   { "timeStamp", DOMEvent::TimeStamp, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "cancelBubble", DOMEvent::CancelBubble, DontDelete, 0, &DOMEventTableEntries[16] },
   { "bubbles", DOMEvent::Bubbles, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "returnValue", DOMEvent::ReturnValue, DontDelete, 0, 0 },
   { "type", DOMEvent::Type, DontDelete|ReadOnly, 0, &DOMEventTableEntries[12] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "srcElement", DOMEvent::SrcElement, DontDelete|ReadOnly, 0, &DOMEventTableEntries[14] },
   { "target", DOMEvent::Target, DontDelete|ReadOnly, 0, &DOMEventTableEntries[13] },
   { "currentTarget", DOMEvent::CurrentTarget, DontDelete|ReadOnly, 0, 0 },
   { "eventPhase", DOMEvent::EventPhase, DontDelete|ReadOnly, 0, &DOMEventTableEntries[15] },
   { "cancelable", DOMEvent::Cancelable, DontDelete|ReadOnly, 0, 0 },
   { "dataTransfer", DOMEvent::DataTransfer, DontDelete|ReadOnly, 0, 0 },
   { "clipboardData", DOMEvent::ClipboardData, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMEventTable = { 2, 17, DOMEventTableEntries, 12 };

} // namespace

namespace KJS {

const struct HashEntry DOMEventProtoTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "stopPropagation", DOMEvent::StopPropagation, DontDelete|Function, 0, &DOMEventProtoTableEntries[3] },
   { "preventDefault", DOMEvent::PreventDefault, DontDelete|Function, 0, &DOMEventProtoTableEntries[4] },
   { "initEvent", DOMEvent::InitEvent, DontDelete|Function, 3, 0 }
};

const struct HashTable DOMEventProtoTable = { 2, 5, DOMEventProtoTableEntries, 3 };

} // namespace

namespace KJS {

const struct HashEntry EventExceptionConstructorTableEntries[] = {
   { "UNSPECIFIED_EVENT_TYPE_ERR", DOM::EventException::UNSPECIFIED_EVENT_TYPE_ERR, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable EventExceptionConstructorTable = { 2, 1, EventExceptionConstructorTableEntries, 1 };

} // namespace

namespace KJS {

const struct HashEntry DOMUIEventTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "charCode", DOMUIEvent::CharCode, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "view", DOMUIEvent::View, DontDelete|ReadOnly, 0, &DOMUIEventTableEntries[8] },
   { "keyCode", DOMUIEvent::KeyCode, DontDelete|ReadOnly, 0, 0 },
   { "layerX", DOMUIEvent::LayerX, DontDelete|ReadOnly, 0, &DOMUIEventTableEntries[9] },
   { "layerY", DOMUIEvent::LayerY, DontDelete|ReadOnly, 0, &DOMUIEventTableEntries[10] },
   { 0, 0, 0, 0, 0 },
   { "detail", DOMUIEvent::Detail, DontDelete|ReadOnly, 0, &DOMUIEventTableEntries[11] },
   { "pageX", DOMUIEvent::PageX, DontDelete|ReadOnly, 0, 0 },
   { "pageY", DOMUIEvent::PageY, DontDelete|ReadOnly, 0, 0 },
   { "which", DOMUIEvent::Which, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMUIEventTable = { 2, 12, DOMUIEventTableEntries, 8 };

} // namespace

namespace KJS {

const struct HashEntry DOMUIEventProtoTableEntries[] = {
   { "initUIEvent", DOMUIEvent::InitUIEvent, DontDelete|Function, 5, 0 }
};

const struct HashTable DOMUIEventProtoTable = { 2, 1, DOMUIEventProtoTableEntries, 1 };

} // namespace

namespace KJS {

const struct HashEntry DOMMouseEventTableEntries[] = {
   { "offsetY", DOMMouseEvent::OffsetY, DontDelete|ReadOnly, 0, &DOMMouseEventTableEntries[20] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "clientX", DOMMouseEvent::ClientX, DontDelete|ReadOnly, 0, &DOMMouseEventTableEntries[19] },
   { "screenX", DOMMouseEvent::ScreenX, DontDelete|ReadOnly, 0, &DOMMouseEventTableEntries[16] },
   { "screenY", DOMMouseEvent::ScreenY, DontDelete|ReadOnly, 0, &DOMMouseEventTableEntries[18] },
   { "altKey", DOMMouseEvent::AltKey, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "button", DOMMouseEvent::Button, DontDelete|ReadOnly, 0, 0 },
   { "toElement", DOMMouseEvent::ToElement, DontDelete|ReadOnly, 0, 0 },
   { "ctrlKey", DOMMouseEvent::CtrlKey, DontDelete|ReadOnly, 0, &DOMMouseEventTableEntries[22] },
   { "offsetX", DOMMouseEvent::OffsetX, DontDelete|ReadOnly, 0, 0 },
   { "x", DOMMouseEvent::X, DontDelete|ReadOnly, 0, &DOMMouseEventTableEntries[17] },
   { "clientY", DOMMouseEvent::ClientY, DontDelete|ReadOnly, 0, &DOMMouseEventTableEntries[21] },
   { "y", DOMMouseEvent::Y, DontDelete|ReadOnly, 0, 0 },
   { "shiftKey", DOMMouseEvent::ShiftKey, DontDelete|ReadOnly, 0, 0 },
   { "metaKey", DOMMouseEvent::MetaKey, DontDelete|ReadOnly, 0, 0 },
   { "relatedTarget", DOMMouseEvent::RelatedTarget, DontDelete|ReadOnly, 0, 0 },
   { "fromElement", DOMMouseEvent::FromElement, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMMouseEventTable = { 2, 23, DOMMouseEventTableEntries, 16 };

} // namespace

namespace KJS {

const struct HashEntry DOMMouseEventProtoTableEntries[] = {
   { "initMouseEvent", DOMMouseEvent::InitMouseEvent, DontDelete|Function, 15, 0 }
};

const struct HashTable DOMMouseEventProtoTable = { 2, 1, DOMMouseEventProtoTableEntries, 1 };

} // namespace

namespace KJS {

const struct HashEntry DOMKeyboardEventTableEntries[] = {
   { "metaKey", DOMKeyboardEvent::MetaKey, DontDelete|ReadOnly, 0, 0 },
   { "keyIdentifier", DOMKeyboardEvent::KeyIdentifier, DontDelete|ReadOnly, 0, &DOMKeyboardEventTableEntries[7] },
   { 0, 0, 0, 0, 0 },
   { "altKey", DOMKeyboardEvent::AltKey, DontDelete|ReadOnly, 0, 0 },
   { "keyLocation", DOMKeyboardEvent::KeyLocation, DontDelete|ReadOnly, 0, &DOMKeyboardEventTableEntries[5] },
   { "ctrlKey", DOMKeyboardEvent::CtrlKey, DontDelete|ReadOnly, 0, &DOMKeyboardEventTableEntries[6] },
   { "shiftKey", DOMKeyboardEvent::ShiftKey, DontDelete|ReadOnly, 0, 0 },
   { "altGraphKey", DOMKeyboardEvent::AltGraphKey, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMKeyboardEventTable = { 2, 8, DOMKeyboardEventTableEntries, 5 };

} // namespace

namespace KJS {

const struct HashEntry DOMKeyboardEventProtoTableEntries[] = {
   { "initKeyboardEvent", DOMKeyboardEvent::InitKeyboardEvent, DontDelete|Function, 11, 0 }
};

const struct HashTable DOMKeyboardEventProtoTable = { 2, 1, DOMKeyboardEventProtoTableEntries, 1 };

} // namespace

namespace KJS {

const struct HashEntry MutationEventConstructorTableEntries[] = {
   { "ADDITION", DOM::MutationEvent::ADDITION, DontDelete|ReadOnly, 0, &MutationEventConstructorTableEntries[3] },
   { "MODIFICATION", DOM::MutationEvent::MODIFICATION, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "REMOVAL", DOM::MutationEvent::REMOVAL, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable MutationEventConstructorTable = { 2, 4, MutationEventConstructorTableEntries, 3 };

} // namespace

namespace KJS {

const struct HashEntry DOMMutationEventTableEntries[] = {
   { "attrChange", DOMMutationEvent::AttrChange, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "relatedNode", DOMMutationEvent::RelatedNode, DontDelete|ReadOnly, 0, 0 },
   { "attrName", DOMMutationEvent::AttrName, DontDelete|ReadOnly, 0, 0 },
   { "prevValue", DOMMutationEvent::PrevValue, DontDelete|ReadOnly, 0, &DOMMutationEventTableEntries[5] },
   { "newValue", DOMMutationEvent::NewValue, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMMutationEventTable = { 2, 6, DOMMutationEventTableEntries, 5 };

} // namespace

namespace KJS {

const struct HashEntry DOMMutationEventProtoTableEntries[] = {
   { "initMutationEvent", DOMMutationEvent::InitMutationEvent, DontDelete|Function, 8, 0 }
};

const struct HashTable DOMMutationEventProtoTable = { 2, 1, DOMMutationEventProtoTableEntries, 1 };

} // namespace

namespace KJS {

const struct HashEntry DOMWheelEventTableEntries[] = {
   { "metaKey", DOMWheelEvent::MetaKey, DontDelete|ReadOnly, 0, &DOMWheelEventTableEntries[13] },
   { "y", DOMWheelEvent::Y, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "wheelDelta", DOMWheelEvent::WheelDelta, DontDelete|ReadOnly, 0, 0 },
   { "ctrlKey", DOMWheelEvent::CtrlKey, DontDelete|ReadOnly, 0, 0 },
   { "offsetX", DOMWheelEvent::OffsetX, DontDelete|ReadOnly, 0, 0 },
   { "offsetY", DOMWheelEvent::OffsetY, DontDelete|ReadOnly, 0, 0 },
   { "clientX", DOMWheelEvent::ClientX, DontDelete|ReadOnly, 0, 0 },
   { "altKey", DOMWheelEvent::AltKey, DontDelete|ReadOnly, 0, &DOMWheelEventTableEntries[10] },
   { "screenY", DOMWheelEvent::ScreenY, DontDelete|ReadOnly, 0, &DOMWheelEventTableEntries[12] },
   { "clientY", DOMWheelEvent::ClientY, DontDelete|ReadOnly, 0, &DOMWheelEventTableEntries[11] },
   { "screenX", DOMWheelEvent::ScreenX, DontDelete|ReadOnly, 0, 0 },
   { "shiftKey", DOMWheelEvent::ShiftKey, DontDelete|ReadOnly, 0, 0 },
   { "x", DOMWheelEvent::X, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMWheelEventTable = { 2, 14, DOMWheelEventTableEntries, 10 };

} // namespace

namespace KJS {

const struct HashEntry DOMWheelEventProtoTableEntries[] = {
};

const struct HashTable DOMWheelEventProtoTable = { 2, 1, DOMWheelEventProtoTableEntries, 1 };

} // namespace

namespace KJS {

const struct HashEntry ClipboardTableEntries[] = {
   { "dropEffect", Clipboard::DropEffect, DontDelete, 0, 0 },
   { "effectAllowed", Clipboard::EffectAllowed, DontDelete, 0, &ClipboardTableEntries[3] },
   { 0, 0, 0, 0, 0 },
   { "types", Clipboard::Types, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable ClipboardTable = { 2, 4, ClipboardTableEntries, 3 };

} // namespace

namespace KJS {

const struct HashEntry ClipboardProtoTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "clearData", Clipboard::ClearData, DontDelete|Function, 0, &ClipboardProtoTableEntries[5] },
   { "getData", Clipboard::GetData, DontDelete|Function, 1, &ClipboardProtoTableEntries[4] },
   { 0, 0, 0, 0, 0 },
   { "setData", Clipboard::SetData, DontDelete|Function, 2, 0 },
   { "setDragImage", Clipboard::SetDragImage, DontDelete|Function, 3, 0 }
};

const struct HashTable ClipboardProtoTable = { 2, 6, ClipboardProtoTableEntries, 4 };

} // namespace
