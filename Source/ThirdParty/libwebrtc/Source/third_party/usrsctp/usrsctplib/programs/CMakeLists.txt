#
# Copyright (C) 2015-2015 Oleg Alexeenkov
# Copyright (C) 2015-2015 Felix Weinrank
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

cmake_minimum_required(VERSION 2.6)
include_directories(../usrsctplib)


#################################################
# INCLUDE MODULES
#################################################

include(CheckFunctionExists)
include(CheckStructHasMember)
include(CheckIncludeFile)
include(CMakePushCheckState)
include(CheckTypeSize)


#################################################
# CHECK FOR TYPES AND FUNCTIONS
#################################################

# xxx warum machen wir die checks?
check_type_size("size_t" HAVE_SIZE_T)
check_type_size("ssize_t" HAVE_SSIZE_T)

check_function_exists("socket" HAVE_SOCKET)
check_function_exists("inet_addr" HAVE_INET_ADDR)


#################################################
# CHECK INCLUDES
#################################################

check_include_file(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/queue.h HAVE_SYS_QUEUE_H)
check_include_file(linux/if_addr.h HAVE_LINUX_IF_ADDR_H)
check_include_file(linux/rtnetlink.h HAVE_LINUX_RTNETLINK_H)
check_include_file(netinet/ip_icmp.h HAVE_NETINET_IP_ICMP_H)
check_include_file(usrsctp.h HAVE_USRSCTP_H)


#################################################
# CHECK STRUCT MEMBERS
#################################################

check_struct_has_member("struct sockaddr" "sa_len" "sys/types.h;sys/socket.h" HAVE_SA_LEN)
if(HAVE_SA_LEN)
	add_definitions(-DHAVE_SA_LEN)
endif()

check_struct_has_member("struct sockaddr_in" "sin_len" "sys/types.h;netinet/in.h" HAVE_SIN_LEN)
if(HAVE_SIN_LEN)
	add_definitions(-DHAVE_SIN_LEN)
endif()

check_struct_has_member("struct sockaddr_in6" "sin6_len" "sys/types.h;netinet/in.h" HAVE_SIN6_LEN)
if(HAVE_SIN6_LEN)
	add_definitions(-DHAVE_SIN6_LEN)
endif()

check_struct_has_member("struct sockaddr_conn" "sconn_len" "usrsctp.h" HAVE_SCONN_LEN)
if(HAVE_SCONN_LEN)
	add_definitions(-DHAVE_SCONN_LEN)
endif()


#################################################
# CHECK OPTIONS
#################################################

option(SCTP_DEBUG "Provide debug information" 1)
if (SCTP_DEBUG)
	add_definitions(-DSCTP_DEBUG)
endif()

option(INET "Support IPv4 " 1)
if (INET)
	add_definitions(-DINET)
endif()

option(INET6 "Support IPv6 " 1)
if (INET6)
	add_definitions(-DINET6)
endif()

option(LINK_STATIC "Link static" 0)

# xxx enable W32 support for shared lib ...
if (LINK_STATIC OR WIN32)
	set(LINK_STATIC "usrsctp-static")
else()
	set(LINK_STATIC "usrsctp")
endif()

option(WERROR "Warning as error" ON)


#################################################
# OS DEPENDENT
#################################################

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-D_GNU_SOURCE)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	add_definitions(-D__APPLE_USE_RFC_2292)
endif()


#################################################
# MISC
#################################################

FIND_PACKAGE(Threads)


#################################################
# PROGRAMS
#################################################

SET (check_PROGRAMS
	client.c
	datachan_serv.c
	daytime_server.c
	discard_server.c
	echo_server.c
	ekr_client.c
	ekr_loop.c
	ekr_peer.c
	ekr_server.c
	http_client.c
	rtcweb.c
	test_libmgmt.c
	tsctp.c
)

# SETTINGS FOR UNIX COMPILER
IF ("x${CMAKE_C_COMPILER_ID}" STREQUAL "xClang" OR "x${CMAKE_C_COMPILER_ID}" STREQUAL "xGNU")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -std=c99")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -std=c99")
	if (WERROR)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	endif()
ENDIF ()

# SETTINGS FOR VISUAL STUDIO COMPILER
IF ("x${CMAKE_C_COMPILER_ID}" STREQUAL "xMSVC")
	IF (CMAKE_C_FLAGS MATCHES "/W[0-4]")
		STRING(REGEX REPLACE "/W[0-4]" "/W3" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	ELSE ()
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
	ENDIF ()

	IF (WERROR)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
	ENDIF()
ENDIF ()

FOREACH (source_file ${check_PROGRAMS})
	GET_FILENAME_COMPONENT (source_file_we ${source_file} NAME_WE)
	ADD_EXECUTABLE (
		${source_file_we}
		${source_file}
	)

	TARGET_LINK_LIBRARIES (${source_file_we}
		${LINK_STATIC}
		${CMAKE_THREAD_LIBS_INIT}
	)

	ADD_TEST (${source_file_we} ${source_file_we})
ENDFOREACH ()
