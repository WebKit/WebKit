/* Automatically generated from array_object.cpp using ./create_hash_table. DO NOT EDIT ! */

#include "lookup.h"

namespace KJS {

const struct HashEntry arrayTableEntries[] = {
   { "toString", ArrayProtoFuncImp::ToString, DontEnum|Function, 0, 0 },
   { "sort", ArrayProtoFuncImp::Sort, DontEnum|Function, 1, 0 },
   { "unshift", ArrayProtoFuncImp::UnShift, DontEnum|Function, 1, 0 },
   { "join", ArrayProtoFuncImp::Join, DontEnum|Function, 1, &arrayTableEntries[15] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "push", ArrayProtoFuncImp::Push, DontEnum|Function, 1, 0 },
   { "toLocaleString", ArrayProtoFuncImp::ToLocaleString, DontEnum|Function, 0, 0 },
   { "concat", ArrayProtoFuncImp::Concat, DontEnum|Function, 1, &arrayTableEntries[14] },
   { "shift", ArrayProtoFuncImp::Shift, DontEnum|Function, 0, 0 },
   { "pop", ArrayProtoFuncImp::Pop, DontEnum|Function, 0, &arrayTableEntries[13] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "reverse", ArrayProtoFuncImp::Reverse, DontEnum|Function, 0, 0 },
   { "slice", ArrayProtoFuncImp::Slice, DontEnum|Function, 2, 0 },
   { "splice", ArrayProtoFuncImp::Splice, DontEnum|Function, 2, 0 }
};

const struct HashTable arrayTable = { 2, 16, arrayTableEntries, 13 };

} // namespace
