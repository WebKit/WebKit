/* Automatically generated from kjs_dom.cpp using ../../../JavaScriptCore/kjs/create_hash_table. DO NOT EDIT ! */

namespace KJS {

const struct HashEntry DOMNodeProtoTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "hasAttributes", DOMNode::HasAttributes, DontDelete|Function, 0, 0 },
   { "normalize", DOMNode::Normalize, DontDelete|Function, 0, &DOMNodeProtoTableEntries[16] },
   { "isSupported", DOMNode::IsSupported, DontDelete|Function, 2, 0 },
   { "removeEventListener", DOMNode::RemoveEventListener, DontDelete|Function, 3, 0 },
   { "hasChildNodes", DOMNode::HasChildNodes, DontDelete|Function, 0, &DOMNodeProtoTableEntries[15] },
   { 0, 0, 0, 0, 0 },
   { "replaceChild", DOMNode::ReplaceChild, DontDelete|Function, 2, &DOMNodeProtoTableEntries[13] },
   { "insertBefore", DOMNode::InsertBefore, DontDelete|Function, 2, 0 },
   { "cloneNode", DOMNode::CloneNode, DontDelete|Function, 1, 0 },
   { "dispatchEvent", DOMNode::DispatchEvent, DontDelete|Function, 1, 0 },
   { "appendChild", DOMNode::AppendChild, DontDelete|Function, 1, &DOMNodeProtoTableEntries[14] },
   { 0, 0, 0, 0, 0 },
   { "removeChild", DOMNode::RemoveChild, DontDelete|Function, 1, 0 },
   { "addEventListener", DOMNode::AddEventListener, DontDelete|Function, 3, 0 },
   { "contains", DOMNode::Contains, DontDelete|Function, 1, 0 },
   { "item", DOMNode::Item, DontDelete|Function, 1, 0 }
};

const struct HashTable DOMNodeProtoTable = { 2, 17, DOMNodeProtoTableEntries, 13 };

} // namespace

namespace KJS {

const struct HashEntry DOMNodeTableEntries[] = {
   { "ondragdrop", DOMNode::OnDragDrop, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onclick", DOMNode::OnClick, DontDelete, 0, 0 },
   { "nodeName", DOMNode::NodeName, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onscroll", DOMNode::OnScroll, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "ondragover", DOMNode::OnDragOver, DontDelete, 0, &DOMNodeTableEntries[76] },
   { "ondragend", DOMNode::OnDragEnd, DontDelete, 0, &DOMNodeTableEntries[75] },
   { 0, 0, 0, 0, 0 },
   { "onsubmit", DOMNode::OnSubmit, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onmouseover", DOMNode::OnMouseOver, DontDelete, 0, &DOMNodeTableEntries[82] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "childNodes", DOMNode::ChildNodes, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[85] },
   { "oncut", DOMNode::OnCut, DontDelete, 0, 0 },
   { "onbeforecopy", DOMNode::OnBeforeCopy, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "nextSibling", DOMNode::NextSibling, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[67] },
   { "ondragleave", DOMNode::OnDragLeave, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "attributes", DOMNode::Attributes, DontDelete|ReadOnly, 0, 0 },
   { "parentElement", DOMNode::ParentElement, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[69] },
   { "onpaste", DOMNode::OnPaste, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onfocus", DOMNode::OnFocus, DontDelete, 0, &DOMNodeTableEntries[91] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "firstChild", DOMNode::FirstChild, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[68] },
   { "ondrag", DOMNode::OnDrag, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onload", DOMNode::OnLoad, DontDelete, 0, &DOMNodeTableEntries[78] },
   { "parentNode", DOMNode::ParentNode, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[73] },
   { "nodeType", DOMNode::NodeType, DontDelete|ReadOnly, 0, 0 },
   { "localName", DOMNode::LocalName, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[84] },
   { "ondragenter", DOMNode::OnDragEnter, DontDelete, 0, &DOMNodeTableEntries[72] },
   { 0, 0, 0, 0, 0 },
   { "ondblclick", DOMNode::OnDblClick, DontDelete, 0, 0 },
   { "onbeforecut", DOMNode::OnBeforeCut, DontDelete, 0, 0 },
   { "namespaceURI", DOMNode::NamespaceURI, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[74] },
   { 0, 0, 0, 0, 0 },
   { "oninput", DOMNode::OnInput, DontDelete, 0, 0 },
   { "scrollLeft", DOMNode::ScrollLeft, DontDelete, 0, 0 },
   { "ownerDocument", DOMNode::OwnerDocument, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[77] },
   { "onsearch", DOMNode::OnSearch, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "lastChild", DOMNode::LastChild, DontDelete|ReadOnly, 0, &DOMNodeTableEntries[70] },
   { "scrollHeight", DOMNode::ScrollHeight, DontDelete|ReadOnly, 0, 0 },
   { "prefix", DOMNode::Prefix, DontDelete, 0, 0 },
   { "onkeydown", DOMNode::OnKeyDown, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "ondragstart", DOMNode::OnDragStart, DontDelete, 0, 0 },
   { "onblur", DOMNode::OnBlur, DontDelete, 0, &DOMNodeTableEntries[71] },
   { 0, 0, 0, 0, 0 },
   { "onmove", DOMNode::OnMove, DontDelete, 0, &DOMNodeTableEntries[81] },
   { 0, 0, 0, 0, 0 },
   { "offsetParent", DOMNode::OffsetParent, DontDelete|ReadOnly, 0, 0 },
   { "nodeValue", DOMNode::NodeValue, DontDelete, 0, &DOMNodeTableEntries[83] },
   { "oncopy", DOMNode::OnCopy, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "previousSibling", DOMNode::PreviousSibling, DontDelete|ReadOnly, 0, 0 },
   { "onmouseup", DOMNode::OnMouseUp, DontDelete, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "onabort", DOMNode::OnAbort, DontDelete, 0, &DOMNodeTableEntries[86] },
   { "onchange", DOMNode::OnChange, DontDelete, 0, &DOMNodeTableEntries[79] },
   { "oncontextmenu", DOMNode::OnContextMenu, DontDelete, 0, &DOMNodeTableEntries[90] },
   { "onbeforepaste", DOMNode::OnBeforePaste, DontDelete, 0, 0 },
   { "ondrop", DOMNode::OnDrop, DontDelete, 0, 0 },
   { "onerror", DOMNode::OnError, DontDelete, 0, 0 },
   { "onkeypress", DOMNode::OnKeyPress, DontDelete, 0, 0 },
   { "onkeyup", DOMNode::OnKeyUp, DontDelete, 0, &DOMNodeTableEntries[87] },
   { "onmousedown", DOMNode::OnMouseDown, DontDelete, 0, &DOMNodeTableEntries[80] },
   { "onmousemove", DOMNode::OnMouseMove, DontDelete, 0, 0 },
   { "onmouseout", DOMNode::OnMouseOut, DontDelete, 0, 0 },
   { "onmousewheel", DOMNode::OnMouseWheel, DontDelete, 0, &DOMNodeTableEntries[89] },
   { "onreset", DOMNode::OnReset, DontDelete, 0, 0 },
   { "onresize", DOMNode::OnResize, DontDelete, 0, 0 },
   { "onselect", DOMNode::OnSelect, DontDelete, 0, 0 },
   { "onselectstart", DOMNode::OnSelectStart, DontDelete, 0, &DOMNodeTableEntries[88] },
   { "onunload", DOMNode::OnUnload, DontDelete, 0, 0 },
   { "offsetLeft", DOMNode::OffsetLeft, DontDelete|ReadOnly, 0, 0 },
   { "offsetTop", DOMNode::OffsetTop, DontDelete|ReadOnly, 0, 0 },
   { "offsetWidth", DOMNode::OffsetWidth, DontDelete|ReadOnly, 0, 0 },
   { "offsetHeight", DOMNode::OffsetHeight, DontDelete|ReadOnly, 0, 0 },
   { "clientWidth", DOMNode::ClientWidth, DontDelete|ReadOnly, 0, 0 },
   { "clientHeight", DOMNode::ClientHeight, DontDelete|ReadOnly, 0, 0 },
   { "scrollTop", DOMNode::ScrollTop, DontDelete, 0, 0 },
   { "scrollWidth", DOMNode::ScrollWidth, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMNodeTable = { 2, 92, DOMNodeTableEntries, 67 };

} // namespace

namespace KJS {

const struct HashEntry DOMAttrTableEntries[] = {
   { "specified", DOMAttr::Specified, DontDelete|ReadOnly, 0, 0 },
   { "value", DOMAttr::ValueProperty, DontDelete|ReadOnly, 0, 0 },
   { "name", DOMAttr::Name, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "ownerElement", DOMAttr::OwnerElement, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMAttrTable = { 2, 5, DOMAttrTableEntries, 5 };

} // namespace

namespace KJS {

const struct HashEntry DOMDocumentProtoTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "createEntityReference", DOMDocument::CreateEntityReference, DontDelete|Function, 1, 0 },
   { "getElementById", DOMDocument::GetElementById, DontDelete|Function, 1, &DOMDocumentProtoTableEntries[35] },
   { 0, 0, 0, 0, 0 },
   { "getElementsByTagName", DOMDocument::GetElementsByTagName, DontDelete|Function, 1, &DOMDocumentProtoTableEntries[32] },
   { "queryCommandIndeterm", DOMDocument::QueryCommandIndeterm, DontDelete|Function, 1, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "createElement", DOMDocument::CreateElement, DontDelete|Function, 1, 0 },
   { "queryCommandEnabled", DOMDocument::QueryCommandEnabled, DontDelete|Function, 1, &DOMDocumentProtoTableEntries[36] },
   { "createAttribute", DOMDocument::CreateAttribute, DontDelete|Function, 1, 0 },
   { "createEvent", DOMDocument::CreateEvent, DontDelete|Function, 1, 0 },
   { 0, 0, 0, 0, 0 },
   { "importNode", DOMDocument::ImportNode, DontDelete|Function, 2, &DOMDocumentProtoTableEntries[34] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "createDocumentFragment", DOMDocument::CreateDocumentFragment, DontDelete|Function, 1, &DOMDocumentProtoTableEntries[29] },
   { "createTextNode", DOMDocument::CreateTextNode, DontDelete|Function, 1, &DOMDocumentProtoTableEntries[33] },
   { "elementFromPoint", DOMDocument::ElementFromPoint, DontDelete|Function, 1, &DOMDocumentProtoTableEntries[31] },
   { "createCDATASection", DOMDocument::CreateCDATASection, DontDelete|Function, 1, &DOMDocumentProtoTableEntries[30] },
   { 0, 0, 0, 0, 0 },
   { "execCommand", DOMDocument::ExecCommand, DontDelete|Function, 3, 0 },
   { 0, 0, 0, 0, 0 },
   { "createElementNS", DOMDocument::CreateElementNS, DontDelete|Function, 2, 0 },
   { "createProcessingInstruction", DOMDocument::CreateProcessingInstruction, DontDelete|Function, 1, 0 },
   { "createAttributeNS", DOMDocument::CreateAttributeNS, DontDelete|Function, 2, 0 },
   { 0, 0, 0, 0, 0 },
   { "getOverrideStyle", DOMDocument::GetOverrideStyle, DontDelete|Function, 2, 0 },
   { "createComment", DOMDocument::CreateComment, DontDelete|Function, 1, 0 },
   { "getElementsByTagNameNS", DOMDocument::GetElementsByTagNameNS, DontDelete|Function, 2, 0 },
   { "createRange", DOMDocument::CreateRange, DontDelete|Function, 0, 0 },
   { "createNodeIterator", DOMDocument::CreateNodeIterator, DontDelete|Function, 3, 0 },
   { "createTreeWalker", DOMDocument::CreateTreeWalker, DontDelete|Function, 4, 0 },
   { "queryCommandState", DOMDocument::QueryCommandState, DontDelete|Function, 1, 0 },
   { "queryCommandSupported", DOMDocument::QueryCommandSupported, DontDelete|Function, 1, 0 },
   { "queryCommandValue", DOMDocument::QueryCommandValue, DontDelete|Function, 1, 0 }
};

const struct HashTable DOMDocumentProtoTable = { 2, 37, DOMDocumentProtoTableEntries, 29 };

} // namespace

namespace KJS {

const struct HashEntry DOMDocumentTableEntries[] = {
   { "doctype", DOMDocument::DocType, DontDelete|ReadOnly, 0, &DOMDocumentTableEntries[7] },
   { "documentElement", DOMDocument::DocumentElement, DontDelete|ReadOnly, 0, &DOMDocumentTableEntries[4] },
   { "implementation", DOMDocument::Implementation, DontDelete|ReadOnly, 0, &DOMDocumentTableEntries[6] },
   { "selectedStylesheetSet", DOMDocument::SelectedStylesheetSet, DontDelete, 0, 0 },
   { "styleSheets", DOMDocument::StyleSheets, DontDelete|ReadOnly, 0, &DOMDocumentTableEntries[5] },
   { "preferredStylesheetSet", DOMDocument::PreferredStylesheetSet, DontDelete|ReadOnly, 0, 0 },
   { "readyState", DOMDocument::ReadyState, DontDelete|ReadOnly, 0, 0 },
   { "defaultView", DOMDocument::DefaultView, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMDocumentTable = { 2, 8, DOMDocumentTableEntries, 4 };

} // namespace

namespace KJS {

const struct HashEntry DOMElementProtoTableEntries[] = {
   { "getAttributeNodeNS", DOMElement::GetAttributeNodeNS, DontDelete|Function, 2, 0 },
   { "getAttributeNS", DOMElement::GetAttributeNS, DontDelete|Function, 2, 0 },
   { 0, 0, 0, 0, 0 },
   { "removeAttributeNode", DOMElement::RemoveAttributeNode, DontDelete|Function, 1, 0 },
   { "removeAttribute", DOMElement::RemoveAttribute, DontDelete|Function, 1, &DOMElementProtoTableEntries[17] },
   { "setAttribute", DOMElement::SetAttribute, DontDelete|Function, 2, 0 },
   { "hasAttribute", DOMElement::HasAttribute, DontDelete|Function, 1, &DOMElementProtoTableEntries[19] },
   { "getElementsByTagNameNS", DOMElement::GetElementsByTagNameNS, DontDelete|Function, 2, 0 },
   { 0, 0, 0, 0, 0 },
   { "getAttributeNode", DOMElement::GetAttributeNode, DontDelete|Function, 1, 0 },
   { "getAttribute", DOMElement::GetAttribute, DontDelete|Function, 1, 0 },
   { 0, 0, 0, 0, 0 },
   { "removeAttributeNS", DOMElement::RemoveAttributeNS, DontDelete|Function, 2, &DOMElementProtoTableEntries[18] },
   { "setAttributeNS", DOMElement::SetAttributeNS, DontDelete|Function, 3, 0 },
   { "hasAttributeNS", DOMElement::HasAttributeNS, DontDelete|Function, 2, 0 },
   { 0, 0, 0, 0, 0 },
   { "getElementsByTagName", DOMElement::GetElementsByTagName, DontDelete|Function, 1, 0 },
   { "setAttributeNode", DOMElement::SetAttributeNode, DontDelete|Function, 2, 0 },
   { "setAttributeNodeNS", DOMElement::SetAttributeNodeNS, DontDelete|Function, 1, &DOMElementProtoTableEntries[20] },
   { "scrollByLines", DOMElement::ScrollByLines, DontDelete|Function, 1, 0 },
   { "scrollByPages", DOMElement::ScrollByPages, DontDelete|Function, 1, 0 }
};

const struct HashTable DOMElementProtoTable = { 2, 21, DOMElementProtoTableEntries, 17 };

} // namespace

namespace KJS {

const struct HashEntry DOMElementTableEntries[] = {
   { "style", DOMElement::Style, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "tagName", DOMElement::TagName, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMElementTable = { 2, 3, DOMElementTableEntries, 3 };

} // namespace

namespace KJS {

const struct HashEntry DOMDOMImplementationProtoTableEntries[] = {
   { "createCSSStyleSheet", DOMDOMImplementation::CreateCSSStyleSheet, DontDelete|Function, 2, 0 },
   { 0, 0, 0, 0, 0 },
   { "hasFeature", DOMDOMImplementation::HasFeature, DontDelete|Function, 2, &DOMDOMImplementationProtoTableEntries[5] },
   { "createHTMLDocument", DOMDOMImplementation::CreateHTMLDocument, DontDelete|Function, 1, 0 },
   { "createDocument", DOMDOMImplementation::CreateDocument, DontDelete|Function, 3, 0 },
   { "createDocumentType", DOMDOMImplementation::CreateDocumentType, DontDelete|Function, 3, 0 }
};

const struct HashTable DOMDOMImplementationProtoTable = { 2, 6, DOMDOMImplementationProtoTableEntries, 5 };

} // namespace

namespace KJS {

const struct HashEntry DOMDocumentTypeTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "notations", DOMDocumentType::Notations, DontDelete|ReadOnly, 0, 0 },
   { "publicId", DOMDocumentType::PublicId, DontDelete|ReadOnly, 0, 0 },
   { "name", DOMDocumentType::Name, DontDelete|ReadOnly, 0, &DOMDocumentTypeTableEntries[6] },
   { "systemId", DOMDocumentType::SystemId, DontDelete|ReadOnly, 0, 0 },
   { "entities", DOMDocumentType::Entities, DontDelete|ReadOnly, 0, 0 },
   { "internalSubset", DOMDocumentType::InternalSubset, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMDocumentTypeTable = { 2, 7, DOMDocumentTypeTableEntries, 6 };

} // namespace

namespace KJS {

const struct HashEntry DOMNamedNodeMapProtoTableEntries[] = {
   { "getNamedItem", DOMNamedNodeMap::GetNamedItem, DontDelete|Function, 1, &DOMNamedNodeMapProtoTableEntries[8] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "item", DOMNamedNodeMap::Item, DontDelete|Function, 1, 0 },
   { "setNamedItem", DOMNamedNodeMap::SetNamedItem, DontDelete|Function, 1, &DOMNamedNodeMapProtoTableEntries[7] },
   { 0, 0, 0, 0, 0 },
   { "removeNamedItem", DOMNamedNodeMap::RemoveNamedItem, DontDelete|Function, 1, &DOMNamedNodeMapProtoTableEntries[9] },
   { "getNamedItemNS", DOMNamedNodeMap::GetNamedItemNS, DontDelete|Function, 2, 0 },
   { "setNamedItemNS", DOMNamedNodeMap::SetNamedItemNS, DontDelete|Function, 1, &DOMNamedNodeMapProtoTableEntries[10] },
   { "removeNamedItemNS", DOMNamedNodeMap::RemoveNamedItemNS, DontDelete|Function, 2, 0 }
};

const struct HashTable DOMNamedNodeMapProtoTable = { 2, 11, DOMNamedNodeMapProtoTableEntries, 7 };

} // namespace

namespace KJS {

const struct HashEntry DOMProcessingInstructionTableEntries[] = {
   { "sheet", DOMProcessingInstruction::Sheet, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "target", DOMProcessingInstruction::Target, DontDelete|ReadOnly, 0, &DOMProcessingInstructionTableEntries[3] },
   { "data", DOMProcessingInstruction::Data, DontDelete, 0, 0 }
};

const struct HashTable DOMProcessingInstructionTable = { 2, 4, DOMProcessingInstructionTableEntries, 3 };

} // namespace

namespace KJS {

const struct HashEntry DOMNotationTableEntries[] = {
   { "publicId", DOMNotation::PublicId, DontDelete|ReadOnly, 0, &DOMNotationTableEntries[2] },
   { 0, 0, 0, 0, 0 },
   { "systemId", DOMNotation::SystemId, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMNotationTable = { 2, 3, DOMNotationTableEntries, 2 };

} // namespace

namespace KJS {

const struct HashEntry DOMEntityTableEntries[] = {
   { "publicId", DOMEntity::PublicId, DontDelete|ReadOnly, 0, &DOMEntityTableEntries[2] },
   { "notationName", DOMEntity::NotationName, DontDelete|ReadOnly, 0, 0 },
   { "systemId", DOMEntity::SystemId, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMEntityTable = { 2, 3, DOMEntityTableEntries, 2 };

} // namespace

namespace KJS {

const struct HashEntry NodeConstructorTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "CDATA_SECTION_NODE", DOM::Node::CDATA_SECTION_NODE, DontDelete|ReadOnly, 0, 0 },
   { "ATTRIBUTE_NODE", DOM::Node::ATTRIBUTE_NODE, DontDelete|ReadOnly, 0, &NodeConstructorTableEntries[12] },
   { "DOCUMENT_FRAGMENT_NODE", DOM::Node::DOCUMENT_FRAGMENT_NODE, DontDelete|ReadOnly, 0, 0 },
   { "DOCUMENT_TYPE_NODE", DOM::Node::DOCUMENT_TYPE_NODE, DontDelete|ReadOnly, 0, 0 },
   { "DOCUMENT_NODE", DOM::Node::DOCUMENT_NODE, DontDelete|ReadOnly, 0, 0 },
   { "COMMENT_NODE", DOM::Node::COMMENT_NODE, DontDelete|ReadOnly, 0, 0 },
   { "ENTITY_NODE", DOM::Node::ENTITY_NODE, DontDelete|ReadOnly, 0, &NodeConstructorTableEntries[13] },
   { "ELEMENT_NODE", DOM::Node::ELEMENT_NODE, DontDelete|ReadOnly, 0, 0 },
   { "TEXT_NODE", DOM::Node::TEXT_NODE, DontDelete|ReadOnly, 0, &NodeConstructorTableEntries[11] },
   { "ENTITY_REFERENCE_NODE", DOM::Node::ENTITY_REFERENCE_NODE, DontDelete|ReadOnly, 0, 0 },
   { "PROCESSING_INSTRUCTION_NODE", DOM::Node::PROCESSING_INSTRUCTION_NODE, DontDelete|ReadOnly, 0, 0 },
   { "NOTATION_NODE", DOM::Node::NOTATION_NODE, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable NodeConstructorTable = { 2, 14, NodeConstructorTableEntries, 11 };

} // namespace

namespace KJS {

const struct HashEntry DOMExceptionConstructorTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "WRONG_DOCUMENT_ERR", DOM::DOMException::WRONG_DOCUMENT_ERR, DontDelete|ReadOnly, 0, 0 },
   { "INUSE_ATTRIBUTE_ERR", DOM::DOMException::INUSE_ATTRIBUTE_ERR, DontDelete|ReadOnly, 0, 0 },
   { "INDEX_SIZE_ERR", DOM::DOMException::INDEX_SIZE_ERR, DontDelete|ReadOnly, 0, 0 },
   { "INVALID_CHARACTER_ERR", DOM::DOMException::INVALID_CHARACTER_ERR, DontDelete|ReadOnly, 0, &DOMExceptionConstructorTableEntries[17] },
   { "NAMESPACE_ERR", DOM::DOMException::NAMESPACE_ERR, DontDelete|ReadOnly, 0, 0 },
   { "NO_DATA_ALLOWED_ERR", DOM::DOMException::NO_DATA_ALLOWED_ERR, DontDelete|ReadOnly, 0, &DOMExceptionConstructorTableEntries[16] },
   { "DOMSTRING_SIZE_ERR", DOM::DOMException::DOMSTRING_SIZE_ERR, DontDelete|ReadOnly, 0, 0 },
   { "NOT_FOUND_ERR", DOM::DOMException::NOT_FOUND_ERR, DontDelete|ReadOnly, 0, &DOMExceptionConstructorTableEntries[15] },
   { 0, 0, 0, 0, 0 },
   { "NO_MODIFICATION_ALLOWED_ERR", DOM::DOMException::NO_MODIFICATION_ALLOWED_ERR, DontDelete|ReadOnly, 0, &DOMExceptionConstructorTableEntries[18] },
   { "HIERARCHY_REQUEST_ERR", DOM::DOMException::HIERARCHY_REQUEST_ERR, DontDelete|ReadOnly, 0, 0 },
   { "INVALID_MODIFICATION_ERR", DOM::DOMException::INVALID_MODIFICATION_ERR, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "NOT_SUPPORTED_ERR", DOM::DOMException::NOT_SUPPORTED_ERR, DontDelete|ReadOnly, 0, 0 },
   { "INVALID_STATE_ERR", DOM::DOMException::INVALID_STATE_ERR, DontDelete|ReadOnly, 0, 0 },
   { "SYNTAX_ERR", DOM::DOMException::SYNTAX_ERR, DontDelete|ReadOnly, 0, 0 },
   { "INVALID_ACCESS_ERR", DOM::DOMException::INVALID_ACCESS_ERR, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMExceptionConstructorTable = { 2, 19, DOMExceptionConstructorTableEntries, 15 };

} // namespace

namespace KJS {

const struct HashEntry DOMCharacterDataTableEntries[] = {
   { "data", DOMCharacterData::Data, DontDelete, 0, &DOMCharacterDataTableEntries[2] },
   { 0, 0, 0, 0, 0 },
   { "length", DOMCharacterData::Length, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMCharacterDataTable = { 2, 3, DOMCharacterDataTableEntries, 2 };

} // namespace

namespace KJS {

const struct HashEntry DOMCharacterDataProtoTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "appendData", DOMCharacterData::AppendData, DontDelete|Function, 1, 0 },
   { "insertData", DOMCharacterData::InsertData, DontDelete|Function, 2, 0 },
   { "deleteData", DOMCharacterData::DeleteData, DontDelete|Function, 2, &DOMCharacterDataProtoTableEntries[7] },
   { 0, 0, 0, 0, 0 },
   { "substringData", DOMCharacterData::SubstringData, DontDelete|Function, 2, 0 },
   { "replaceData", DOMCharacterData::ReplaceData, DontDelete|Function, 2, 0 }
};

const struct HashTable DOMCharacterDataProtoTable = { 2, 8, DOMCharacterDataProtoTableEntries, 7 };

} // namespace

namespace KJS {

const struct HashEntry DOMTextProtoTableEntries[] = {
   { "splitText", DOMText::SplitText, DontDelete|Function, 1, 0 }
};

const struct HashTable DOMTextProtoTable = { 2, 1, DOMTextProtoTableEntries, 1 };

} // namespace
