/* Automatically generated from math_object.cpp using ./create_hash_table. DO NOT EDIT ! */

#include "lookup.h"

namespace KJS {

const struct HashEntry mathTableEntries[] = {
   { "atan", MathObjectImp::ATan, DontEnum|Function, 1, &mathTableEntries[25] },
   { 0, 0, 0, 0, 0 },
   { "SQRT2", MathObjectImp::Sqrt2, DontEnum|DontDelete|ReadOnly, 0, &mathTableEntries[23] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "E", MathObjectImp::Euler, DontEnum|DontDelete|ReadOnly, 0, &mathTableEntries[21] },
   { "asin", MathObjectImp::ASin, DontEnum|Function, 1, &mathTableEntries[26] },
   { "atan2", MathObjectImp::ATan2, DontEnum|Function, 2, &mathTableEntries[32] },
   { "LOG2E", MathObjectImp::Log2E, DontEnum|DontDelete|ReadOnly, 0, &mathTableEntries[27] },
   { "cos", MathObjectImp::Cos, DontEnum|Function, 1, 0 },
   { "max", MathObjectImp::Max, DontEnum|Function, 2, &mathTableEntries[29] },
   { 0, 0, 0, 0, 0 },
   { 0, 0, 0, 0, 0 },
   { "LOG10E", MathObjectImp::Log10E, DontEnum|DontDelete|ReadOnly, 0, &mathTableEntries[24] },
   { "LN2", MathObjectImp::Ln2, DontEnum|DontDelete|ReadOnly, 0, &mathTableEntries[31] },
   { "abs", MathObjectImp::Abs, DontEnum|Function, 1, 0 },
   { "sqrt", MathObjectImp::Sqrt, DontEnum|Function, 1, 0 },
   { "exp", MathObjectImp::Exp, DontEnum|Function, 1, 0 },
   { 0, 0, 0, 0, 0 },
   { "LN10", MathObjectImp::Ln10, DontEnum|DontDelete|ReadOnly, 0, &mathTableEntries[22] },
   { "PI", MathObjectImp::Pi, DontEnum|DontDelete|ReadOnly, 0, &mathTableEntries[28] },
   { "SQRT1_2", MathObjectImp::Sqrt1_2, DontEnum|DontDelete|ReadOnly, 0, 0 },
   { "acos", MathObjectImp::ACos, DontEnum|Function, 1, 0 },
   { "ceil", MathObjectImp::Ceil, DontEnum|Function, 1, 0 },
   { "floor", MathObjectImp::Floor, DontEnum|Function, 1, 0 },
   { "log", MathObjectImp::Log, DontEnum|Function, 1, 0 },
   { "min", MathObjectImp::Min, DontEnum|Function, 2, 0 },
   { "pow", MathObjectImp::Pow, DontEnum|Function, 2, &mathTableEntries[30] },
   { "random", MathObjectImp::Random, DontEnum|Function, 0, 0 },
   { "round", MathObjectImp::Round, DontEnum|Function, 1, 0 },
   { "sin", MathObjectImp::Sin, DontEnum|Function, 1, 0 },
   { "tan", MathObjectImp::Tan, DontEnum|Function, 1, 0 }
};

const struct HashTable mathTable = { 2, 33, mathTableEntries, 21 };

} // namespace
