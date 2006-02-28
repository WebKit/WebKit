// This file exists to work around a bug in Visual Studio 2005
// Occasionally on launch VS05 will fail to correctly find and
// identify generated files, thereby excluding them from the build
// list and defaulting the "build rule" to "custom build rule"
// instead of whatever may be appropriate for the file type.
#include "grammar.cpp"
