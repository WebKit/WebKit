# XML parser

set(XEXML_SRCS
	xeDefs.cpp
	xeDefs.hpp
	xeXMLParser.cpp
	xeXMLParser.hpp
	)

set(XEXML_LIBS
	decpp
	deutil
	debase
	)

add_library(xexml STATIC ${XEXML_SRCS})
target_link_libraries(xexml ${XEXML_LIBS})

