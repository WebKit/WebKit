/* Automatically generated from number_object.cpp using ./create_hash_table. DO NOT EDIT ! */

#include "lookup.h"

namespace KJS {

const struct HashEntry numberTableEntries[] = {
   { "POSITIVE_INFINITY", NumberObjectImp::PosInfinity, DontEnum, 0, 0 },
   { "MAX_VALUE", NumberObjectImp::MaxValue, DontEnum, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "NaN", NumberObjectImp::NaNValue, DontEnum, 0, &numberTableEntries[5] },
   { "MIN_VALUE", NumberObjectImp::MinValue, DontEnum, 0, 0 },
   { "NEGATIVE_INFINITY", NumberObjectImp::NegInfinity, DontEnum, 0, 0 }
};

const struct HashTable numberTable = { 2, 6, numberTableEntries, 5 };

} // namespace
