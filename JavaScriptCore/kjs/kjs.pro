LANGUAGE = C++
TARGET = kjs
TEMPLATE = lib
CONFIG += qt warn_on debug static
QT -= gui

unix {
	!system(which perl >/dev/null 2>&1):error("Build requires perl.")
	!exists(lexer.lut.h):system(perl create_hash_table keywords.table -i >lexer.lut.h)
	!exists(array_object.lut.h):system(perl create_hash_table array_object.cpp -i >array_object.lut.h)
	!exists(math_object.lut.h):system(perl create_hash_table math_object.cpp -i >math_object.lut.h)
	!exists(date_object.lut.h):system(perl create_hash_table date_object.cpp -i >date_object.lut.h)
	!exists(number_object.lut.h):system(perl create_hash_table number_object.cpp -i >number_object.lut.h)
	!exists(string_object.lut.h):system(perl create_hash_table string_object.cpp -i >string_object.lut.h)
	!exists(regexp_object.lut.h):system(perl create_hash_table regexp_object.cpp -i >regexp_object.lut.h)
}

# QMakes YACC support is strange
YACCSOURCES += grammar.y

SOURCES	+= \
	../kxmlcore/FastMalloc.cpp \
	../kxmlcore/TCSystemAlloc.cpp \
	array_object.cpp \
	function_object.cpp \
	nodes2string.cpp \
	reference.cpp \
	bool_object.cpp \
	identifier.cpp \
	nodes.cpp \
	reference_list.cpp \
	collector.cpp \
	internal.cpp \
	number_object.cpp \
	regexp.cpp \
	date_object.cpp \
	interpreter.cpp \
	object.cpp \
	regexp_object.cpp \
	debugger.cpp \
	object_object.cpp \
	scope_chain.cpp \
	dtoa.cpp \
	lexer.cpp \
	operations.cpp \
	string_object.cpp \
	error_object.cpp \
	list.cpp \
	property_map.cpp \
	fpconst.cpp \
	lookup.cpp \
	property_slot.cpp \
	ustring.cpp \
	function.cpp \
	math_object.cpp \
	value.cpp

!macx:unix {
	INCLUDEPATH += .. ../pcre ../kxmlcore
	MOC_DIR = .moc
	OBJECTS_DIR = .obj
	QMAKE_CXXFLAGS_DEBUG += -ansi
}

win32 {
	INCLUDEPATH += .. ../pcre ../kxmlcore ../icu
	QMAKE_CXXFLAGS_RELEASE += /Zm1000
	QMAKE_CXXFLAGS_DEBUG += /Zm1000
}

