/* Automatically generated from kjs_range.cpp using ../../../JavaScriptCore/kjs/create_hash_table. DO NOT EDIT ! */

namespace KJS {

const struct HashEntry DOMRangeTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "endOffset", DOMRange::EndOffset, DontDelete|ReadOnly, 0, 0 },
   { "endContainer", DOMRange::EndContainer, DontDelete|ReadOnly, 0, &DOMRangeTableEntries[7] },
   { "startOffset", DOMRange::StartOffset, DontDelete|ReadOnly, 0, 0 },
   { "startContainer", DOMRange::StartContainer, DontDelete|ReadOnly, 0, 0 },
   { "collapsed", DOMRange::Collapsed, DontDelete|ReadOnly, 0, 0 },
   { "commonAncestorContainer", DOMRange::CommonAncestorContainer, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable DOMRangeTable = { 2, 8, DOMRangeTableEntries, 7 };

}; // namespace

namespace KJS {

const struct HashEntry DOMRangeProtoTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "collapse", DOMRange::Collapse, DontDelete|Function, 1, 0 },
   { "cloneRange", DOMRange::CloneRange, DontDelete|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "setEndAfter", DOMRange::SetEndAfter, DontDelete|Function, 1, 0 },
   { "detach", DOMRange::Detach, DontDelete|Function, 0, 0 },
   { "selectNodeContents", DOMRange::SelectNodeContents, DontDelete|Function, 1, &DOMRangeProtoTableEntries[19] },
   { 0, 0, 0, 0, 0 },
   { "setStart", DOMRange::SetStart, DontDelete|Function, 2, &DOMRangeProtoTableEntries[17] },
   { 0, 0, 0, 0, 0 },
   { "selectNode", DOMRange::SelectNode, DontDelete|Function, 1, &DOMRangeProtoTableEntries[21] },
   { "deleteContents", DOMRange::DeleteContents, DontDelete|Function, 0, &DOMRangeProtoTableEntries[20] },
   { "createContextualFragment", DOMRange::CreateContextualFragment, DontDelete|Function, 1, 0 },
   { "setStartAfter", DOMRange::SetStartAfter, DontDelete|Function, 1, 0 },
   { "insertNode", DOMRange::InsertNode, DontDelete|Function, 1, 0 },
   { "cloneContents", DOMRange::CloneContents, DontDelete|Function, 0, 0 },
   { "setEnd", DOMRange::SetEnd, DontDelete|Function, 2, &DOMRangeProtoTableEntries[18] },
   { "setStartBefore", DOMRange::SetStartBefore, DontDelete|Function, 1, &DOMRangeProtoTableEntries[22] },
   { "setEndBefore", DOMRange::SetEndBefore, DontDelete|Function, 1, 0 },
   { "compareBoundaryPoints", DOMRange::CompareBoundaryPoints, DontDelete|Function, 2, 0 },
   { "extractContents", DOMRange::ExtractContents, DontDelete|Function, 0, 0 },
   { "surroundContents", DOMRange::SurroundContents, DontDelete|Function, 1, 0 },
   { "toString", DOMRange::ToString, DontDelete|Function, 0, 0 }
};

const struct HashTable DOMRangeProtoTable = { 2, 23, DOMRangeProtoTableEntries, 17 };

}; // namespace

namespace KJS {

const struct HashEntry RangeConstructorTableEntries[] = {
   { 0, 0, 0, 0, 0 },
   { "START_TO_END", DOM::Range::START_TO_END, DontDelete|ReadOnly, 0, &RangeConstructorTableEntries[5] },
   { 0, 0, 0, 0, 0 },
   { "END_TO_END", DOM::Range::END_TO_END, DontDelete|ReadOnly, 0, 0 },
   { "START_TO_START", DOM::Range::START_TO_START, DontDelete|ReadOnly, 0, 0 },
   { "END_TO_START", DOM::Range::END_TO_START, DontDelete|ReadOnly, 0, 0 }
};

const struct HashTable RangeConstructorTable = { 2, 6, RangeConstructorTableEntries, 5 };

}; // namespace
