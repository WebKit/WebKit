/* Automatically generated from string_object.cpp using ./create_hash_table. DO NOT EDIT ! */

#include "lookup.h"

namespace KJS {

const struct HashEntry stringTableEntries[] = {
   { "toString", StringProtoFuncImp::ToString, DontEnum|Function, 0, 0 },
   { "bold", StringProtoFuncImp::Bold, DontEnum|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "lastIndexOf", StringProtoFuncImp::LastIndexOf, DontEnum|Function, 2, 0 },
   { "replace", StringProtoFuncImp::Replace, DontEnum|Function, 2, 0 },
   { "match", StringProtoFuncImp::Match, DontEnum|Function, 1, &stringTableEntries[27] },
   { "search", StringProtoFuncImp::Search, DontEnum|Function, 1, &stringTableEntries[34] },
   { 0, 0, 0, 0, 0 },
   { "concat", StringProtoFuncImp::Concat, DontEnum|Function, 1, &stringTableEntries[26] },
   { 0, 0, 0, 0, 0 },
   { "split", StringProtoFuncImp::Split, DontEnum|Function, 2, &stringTableEntries[28] },
   { "anchor", StringProtoFuncImp::Anchor, DontEnum|Function, 1, 0 },
   { "charCodeAt", StringProtoFuncImp::CharCodeAt, DontEnum|Function, 1, 0 },
   { "toUpperCase", StringProtoFuncImp::ToUpperCase, DontEnum|Function, 0, 0 },
   { "link", StringProtoFuncImp::Link, DontEnum|Function, 1, 0 },
   { "indexOf", StringProtoFuncImp::IndexOf, DontEnum|Function, 2, 0 },
   { 0, 0, 0, 0, 0 },
   { "small", StringProtoFuncImp::Small, DontEnum|Function, 0, &stringTableEntries[32] },
   { "sub", StringProtoFuncImp::Sub, DontEnum|Function, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "valueOf", StringProtoFuncImp::ValueOf, DontEnum|Function, 0, &stringTableEntries[29] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "charAt", StringProtoFuncImp::CharAt, DontEnum|Function, 1, 0 },
   { "fontsize", StringProtoFuncImp::Fontsize, DontEnum|Function, 1, 0 },
   { "substr", StringProtoFuncImp::Substr, DontEnum|Function, 2, 0 },
   { "slice", StringProtoFuncImp::Slice, DontEnum|Function, 2, &stringTableEntries[30] },
   { "substring", StringProtoFuncImp::Substring, DontEnum|Function, 2, 0 },
   { "toLowerCase", StringProtoFuncImp::ToLowerCase, DontEnum|Function, 0, 0 },
   { "big", StringProtoFuncImp::Big, DontEnum|Function, 0, &stringTableEntries[35] },
   { "blink", StringProtoFuncImp::Blink, DontEnum|Function, 0, &stringTableEntries[31] },
   { "fixed", StringProtoFuncImp::Fixed, DontEnum|Function, 0, &stringTableEntries[33] },
   { "italics", StringProtoFuncImp::Italics, DontEnum|Function, 0, 0 },
   { "strike", StringProtoFuncImp::Strike, DontEnum|Function, 0, 0 },
   { "sup", StringProtoFuncImp::Sup, DontEnum|Function, 0, 0 },
   { "fontcolor", StringProtoFuncImp::Fontcolor, DontEnum|Function, 1, 0 }
};

const struct HashTable stringTable = { 2, 36, stringTableEntries, 26 };

}; // namespace
