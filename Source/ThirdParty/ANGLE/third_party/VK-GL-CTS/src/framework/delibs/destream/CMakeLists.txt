set(DESTERAM_SRCS
	deIOStream.h
	deInStream.h
	deOutStream.h
	deFileStream.h
	deFileStream.c
	deRingbuffer.h
	deRingbuffer.c
	deStreamCpyThread.h
	deStreamCpyThread.c
	deThreadStream.h
	deThreadStream.c
	)

set(DESTREAM_LIBS
	deutil
	)

add_library(destream ${DESTERAM_SRCS})
target_link_libraries(destream ${DESTREAM_LIBS})
