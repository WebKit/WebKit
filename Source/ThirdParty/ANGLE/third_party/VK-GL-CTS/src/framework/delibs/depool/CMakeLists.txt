# cmake file for depool

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

set(DEPOOL_SRCS
	deMemPool.c
	deMemPool.h
	dePoolArray.c
	dePoolArray.h
	dePoolHeap.c
	dePoolHeap.h
	dePoolHash.c
	dePoolHash.h
	dePoolHashArray.c
	dePoolHashArray.h
	dePoolHashSet.c
	dePoolHashSet.h
	dePoolMultiSet.c
	dePoolMultiSet.h
	dePoolSet.c
	dePoolSet.h
	dePoolStringBuilder.c
	dePoolStringBuilder.h
	dePoolTest.c
	dePoolTest.h
	)

if (DE_OS_IS_UNIX OR DE_OS_IS_QNX)
	# vsnprintf
	add_definitions(-D_XOPEN_SOURCE=600)
endif ()

include_directories(../debase)
add_library(depool STATIC ${DEPOOL_SRCS})
target_link_libraries(depool debase)
