/* Automatically generated from kjs_traversal.cpp using ../../../JavaScriptCore/kjs/create_hash_table. DO NOT EDIT ! */

namespace KJS {

const struct HashEntry DOMNodeIteratorTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "filter", DOMNodeIterator::Filter, DontDelete|ReadOnly, 0, 0 },
   { "root", DOMNodeIterator::Root, DontDelete|ReadOnly, 0, 0 },
   { "whatToShow", DOMNodeIterator::WhatToShow, DontDelete|ReadOnly, 0, &DOMNodeIteratorTableEntries[5] },
   { 0, 0, 0, 0, 0 },
   { "expandEntityReferences", DOMNodeIterator::ExpandEntityReferences, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMNodeIteratorTable = { 2, 6, DOMNodeIteratorTableEntries, 5 };

}; // namespace

namespace KJS {

const struct HashEntry DOMNodeIteratorProtoTableEntries[] = {
   { "nextNode", DOMNodeIterator::NextNode, DontDelete|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "previousNode", DOMNodeIterator::PreviousNode, DontDelete|Function, 0, &DOMNodeIteratorProtoTableEntries[3] },
   { "detach", DOMNodeIterator::Detach, DontDelete|Function, 0, 0 }
};

const struct HashTable DOMNodeIteratorProtoTable = { 2, 4, DOMNodeIteratorProtoTableEntries, 3 };

}; // namespace

namespace KJS {

const struct HashEntry NodeFilterConstructorTableEntries[] = {
   { "SHOW_PROCESSING_INSTRUCTION", DOM::NodeFilter::SHOW_PROCESSING_INSTRUCTION, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "SHOW_ELEMENT", DOM::NodeFilter::SHOW_ELEMENT, DontDelete|ReadOnly, 0, &NodeFilterConstructorTableEntries[17] },
   { "SHOW_ALL", DOM::NodeFilter::SHOW_ALL, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "FILTER_REJECT", DOM::NodeFilter::FILTER_REJECT, DontDelete|ReadOnly, 0, 0 },
   { "SHOW_ENTITY", DOM::NodeFilter::SHOW_ENTITY, DontDelete|ReadOnly, 0, 0 },
   { "FILTER_SKIP", DOM::NodeFilter::FILTER_SKIP, DontDelete|ReadOnly, 0, &NodeFilterConstructorTableEntries[18] },
   { "SHOW_ENTITY_REFERENCE", DOM::NodeFilter::SHOW_ENTITY_REFERENCE, DontDelete|ReadOnly, 0, 0 },
   { "FILTER_ACCEPT", DOM::NodeFilter::FILTER_ACCEPT, DontDelete|ReadOnly, 0, &NodeFilterConstructorTableEntries[19] },
   { 0, 0, 0, 0, 0 },
   { "SHOW_DOCUMENT_FRAGMENT", DOM::NodeFilter::SHOW_DOCUMENT_FRAGMENT, DontDelete|ReadOnly, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "SHOW_CDATA_SECTION", DOM::NodeFilter::SHOW_CDATA_SECTION, DontDelete|ReadOnly, 0, &NodeFilterConstructorTableEntries[22] },
   { "SHOW_ATTRIBUTE", DOM::NodeFilter::SHOW_ATTRIBUTE, DontDelete|ReadOnly, 0, &NodeFilterConstructorTableEntries[20] },
   { "SHOW_TEXT", DOM::NodeFilter::SHOW_TEXT, DontDelete|ReadOnly, 0, 0 },
   { "SHOW_COMMENT", DOM::NodeFilter::SHOW_COMMENT, DontDelete|ReadOnly, 0, &NodeFilterConstructorTableEntries[21] },
   { "SHOW_DOCUMENT", DOM::NodeFilter::SHOW_DOCUMENT, DontDelete|ReadOnly, 0, 0 },
   { "SHOW_DOCUMENT_TYPE", DOM::NodeFilter::SHOW_DOCUMENT_TYPE, DontDelete|ReadOnly, 0, 0 },
   { "SHOW_NOTATION", DOM::NodeFilter::SHOW_NOTATION, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable NodeFilterConstructorTable = { 2, 23, NodeFilterConstructorTableEntries, 17 };

}; // namespace

namespace KJS {

const struct HashEntry DOMNodeFilterProtoTableEntries[] = {
   { "acceptNode", DOMNodeFilter::AcceptNode, DontDelete|Function, 0, 0 }
};

const struct HashTable DOMNodeFilterProtoTable = { 2, 1, DOMNodeFilterProtoTableEntries, 1 };

}; // namespace

namespace KJS {

const struct HashEntry DOMTreeWalkerTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "filter", DOMTreeWalker::Filter, DontDelete|ReadOnly, 0, &DOMTreeWalkerTableEntries[6] },
   { "root", DOMTreeWalker::Root, DontDelete|ReadOnly, 0, 0 },
   { "whatToShow", DOMTreeWalker::WhatToShow, DontDelete|ReadOnly, 0, &DOMTreeWalkerTableEntries[5] },
   { 0, 0, 0, 0, 0 },
   { "expandEntityReferences", DOMTreeWalker::ExpandEntityReferences, DontDelete|ReadOnly, 0, 0 },
   { "currentNode", DOMTreeWalker::CurrentNode, DontDelete, 0, 0 }
};

const struct HashTable DOMTreeWalkerTable = { 2, 7, DOMTreeWalkerTableEntries, 5 };

}; // namespace

namespace KJS {

const struct HashEntry DOMTreeWalkerProtoTableEntries[] = {
   { "firstChild", DOMTreeWalker::FirstChild, DontDelete|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "previousSibling", DOMTreeWalker::PreviousSibling, DontDelete|Function, 0, &DOMTreeWalkerProtoTableEntries[8] },
   { "lastChild", DOMTreeWalker::LastChild, DontDelete|Function, 0, 0 },
   { "parentNode", DOMTreeWalker::ParentNode, DontDelete|Function, 0, &DOMTreeWalkerProtoTableEntries[7] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "nextSibling", DOMTreeWalker::NextSibling, DontDelete|Function, 0, &DOMTreeWalkerProtoTableEntries[9] },
   { "previousNode", DOMTreeWalker::PreviousNode, DontDelete|Function, 0, 0 },
   { "nextNode", DOMTreeWalker::NextNode, DontDelete|Function, 0, 0 }
};

const struct HashTable DOMTreeWalkerProtoTable = { 2, 10, DOMTreeWalkerProtoTableEntries, 7 };

}; // namespace
