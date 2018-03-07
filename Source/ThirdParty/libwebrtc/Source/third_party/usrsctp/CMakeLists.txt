#
# Copyright (C) 2015-2015 Oleg Alexeenkov
# Copyright (C) 2015-2017 Felix Weinrank
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
project(usrsctplib)

# Debug build type as default
if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, using DEBUG")
    set(CMAKE_BUILD_TYPE "DEBUG")
endif ()

include(CheckStructHasMember)
include(CheckIncludeFile)
include(CheckIncludeFiles)


#################################################
# CHECK OPTIONS
#################################################

option(INVARIANTS "Add runtime checks" 0)
if (INVARIANTS)
    add_definitions(-DINVARIANTS)
endif ()

option(SCTP_DEBUG "Provide debug information" 1)
if (SCTP_DEBUG)
    add_definitions(-DSCTP_DEBUG)
endif ()

option(INET "Support IPv4" 1)
if (INET)
    add_definitions(-DINET)
endif ()

option(INET6 "Support IPv6" 1)
if (INET6)
    add_definitions(-DINET6)
endif ()

option(SCTP_SIMPLE_ALLOCATOR " " 1)
if (SCTP_SIMPLE_ALLOCATOR)
    add_definitions(-DSCTP_SIMPLE_ALLOCATOR)
endif ()

option(SCTP_PROCESS_LEVEL_LOCKS " " 1)
if (SCTP_PROCESS_LEVEL_LOCKS)
    add_definitions(-DSCTP_PROCESS_LEVEL_LOCKS)
endif ()

option(SCTP_WITH_NO_CSUM "Disable SCTP checksum" 0)
if (SCTP_WITH_NO_CSUM)
    add_definitions(-DSCTP_WITH_NO_CSUM)
endif ()

option(SCTP_MBUF_LOGGING " " 0)
if (SCTP_MBUF_LOGGING)
    add_definitions(-DSCTP_MBUF_LOGGING)
endif ()

option(SCTP_PACKET_LOGGING " " 0)
if (SCTP_PACKET_LOGGING)
    add_definitions(-DSCTP_PACKET_LOGGING)
endif ()

option(SCTP_SO_LOCK_TESTING " " 0)
if (SCTP_SO_LOCK_TESTING)
    add_definitions(-DSCTP_SO_LOCK_TESTING)
endif ()

option(SCTP_EMBEDDED_V6_SCOPE " " 0)
if (SCTP_EMBEDDED_V6_SCOPE)
    add_definitions(-DSCTP_EMBEDDED_V6_SCOPE)
endif ()

option(SCTP_KAME " " 0)
if (SCTP_KAME)
    add_definitions(-DSCTP_KAME)
endif ()

option(WERROR "Warning as error" 1)

option(LINK_PROGRAMS_STATIC "Link example programs static" 0)

option(BUILD_PROGRAMS "Build example programs" 1)

option(SANITIZER_ADDRESS "Compile with address sanitizer" 0)

option(SANITIZER_MEMORY "Compile with memory sanitizer" 0)

if (SANITIZER_ADDRESS AND SANITIZER_MEMORY)
    message(FATAL_ERROR "Can not compile with both sanitizer options")
endif ()

if (LINK_PROGRAMS_STATIC OR WIN32)
    set(PROGRAMS_LINK_LIBRARY "usrsctp-static")
else ()
    set(PROGRAMS_LINK_LIBRARY "usrsctp")
endif ()


#################################################
# CHECK FOR TYPES AND FUNCTIONS
#################################################

check_include_files("sys/queue.h" HAVE_SYS_QUEUE_H)
if (HAVE_SYS_QUEUE_H)
    add_definitions(-DHAVE_SYS_QUEUE_H)
endif ()

check_include_files("sys/socket.h;linux/if_addr.h" HAVE_LINUX_IF_ADDR_H)
if (HAVE_LINUX_IF_ADDR_H)
    add_definitions(-DHAVE_LINUX_IF_ADDR_H)
endif ()

check_include_files("sys/socket.h;linux/rtnetlink.h" HAVE_LINUX_RTNETLINK_H)
if (HAVE_LINUX_RTNETLINK_H)
    add_definitions(-DHAVE_LINUX_RTNETLINK_H)
endif ()

check_include_files("sys/types.h;netinet/in.h;netinet/ip.h;netinet/ip_icmp.h" HAVE_NETINET_IP_ICMP_H)
if (HAVE_NETINET_IP_ICMP_H)
    add_definitions(-DHAVE_NETINET_IP_ICMP_H)
endif ()

check_include_files("stdatomic.h" HAVE_STDATOMIC_H)
if (HAVE_STDATOMIC_H)
    add_definitions(-DHAVE_STDATOMIC_H)
endif ()


#################################################
# CHECK STRUCT MEMBERS
#################################################

set (CMAKE_REQUIRED_INCLUDES "${CMAKE_SOURCE_DIR}/usrsctplib")
check_include_file(usrsctp.h HAVE_USRSCTP_H)
if (NOT HAVE_USRSCTP_H)
    message(FATAL_ERROR "usrsctp.h not found")
endif ()

check_struct_has_member("struct sockaddr" "sa_len" "sys/types.h;sys/socket.h" HAVE_SA_LEN)
if (HAVE_SA_LEN)
    message(STATUS "HAVE_SA_LEN")
    add_definitions(-DHAVE_SA_LEN)
endif ()

check_struct_has_member("struct sockaddr_in" "sin_len" "sys/types.h;netinet/in.h" HAVE_SIN_LEN)
if (HAVE_SIN_LEN)
    message(STATUS "HAVE_SIN_LEN")
    add_definitions(-DHAVE_SIN_LEN)
endif ()

check_struct_has_member("struct sockaddr_in6" "sin6_len" "sys/types.h;netinet/in.h" HAVE_SIN6_LEN)
if (HAVE_SIN6_LEN)
    message(STATUS "HAVE_SIN6_LEN")
    add_definitions(-DHAVE_SIN6_LEN)
endif ()

check_struct_has_member("struct sockaddr_conn" "sconn_len" "usrsctp.h" HAVE_SCONN_LEN)
if (HAVE_SCONN_LEN)
    message(STATUS "HAVE_SCONN_LEN")
    add_definitions(-DHAVE_SCONN_LEN)
endif ()


#################################################
# COMPILER SETTINGS
#################################################

# SETTINGS FOR UNIX COMPILER
if ("x${CMAKE_C_COMPILER_ID}" STREQUAL "xClang" OR "x${CMAKE_C_COMPILER_ID}" STREQUAL "xGNU")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -std=c99 -pedantic -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Winit-self -Wno-unused-function -Wno-unused-parameter")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99 -pedantic -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Winit-self -Wno-unused-function -Wno-unused-parameter")

    if (WERROR)
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Werror")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif ()

    if (SANITIZER_ADDRESS)
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fsanitize=address -O1 -fno-omit-frame-pointer -g -Wno-address-of-packed-member")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -O1 -fno-omit-frame-pointer -g -Wno-address-of-packed-member")
    endif ()

    if (SANITIZER_MEMORY)
        # maybe add "-fPIE -pie" here
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -fsanitize=memory -fno-omit-frame-pointer -g -Wno-address-of-packed-member -fsanitize-memory-track-origins")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -fno-omit-frame-pointer -g -Wno-address-of-packed-member -fsanitize-memory-track-origins")
    endif ()
endif ()

# SETTINGS FOR VISUAL STUDIO COMPILER
if ("x${CMAKE_C_COMPILER_ID}" STREQUAL "xMSVC")
    if (CMAKE_C_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W3" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    else ()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
    endif ()

    if (WERROR)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
    endif ()
endif ()


#################################################
# INCLUDE SUBDIRS
#################################################

add_subdirectory(usrsctplib)

if (BUILD_PROGRAMS)
    add_subdirectory(programs)
endif ()
