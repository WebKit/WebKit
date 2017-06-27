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


#################################################
# INCLUDE MODULES
#################################################

include(CheckFunctionExists)
include(CheckStructHasMember)
include(CheckIncludeFiles)
include(CMakePushCheckState)
include(CheckTypeSize)

set(VERSION "1.0.0")

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include/usrsctp")
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MACOSX_RPATH 1)


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

add_definitions(-D__Userspace__)
add_definitions(-D__Userspace_os_${CMAKE_SYSTEM_NAME})
add_definitions(-D_GNU_SOURCE)


#################################################
# CHECK STRUCT MEMBERS
#################################################

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
# OS DEPENDENT
#################################################

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_definitions(-D_GNU_SOURCE)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    add_definitions(-U__FreeBSD__)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_definitions(-U__APPLE__)
    add_definitions(-D__APPLE_USE_RFC_2292)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
    add_definitions(-U__DragonFly__)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    add_definitions(-U__NetBSD__)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    add_definitions(-U__OpenBSD__)
endif ()


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

option(INET "Support IPv4 " 1)
if (INET)
    add_definitions(-DINET)
endif ()

option(INET6 "Support IPv6 " 1)
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

option(WERROR "Warning as error" ON)

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/netinet ${PROJECT_SOURCE_DIR}/netinet6)

list(APPEND usrsctp_root_HEADERS
    user_atomic.h       user_inpcb.h    user_ip_icmp.h  user_mbuf.h   user_recv_thread.h  user_socketvar.h  usrsctp.h
    user_environment.h  user_ip6_var.h  user_malloc.h   user_queue.h  user_route.h        user_uma.h
)

list(APPEND usrsctp_netinet_HEADERS
    netinet/sctp_asconf.h    netinet/sctp_constants.h  netinet/sctp_indata.h          netinet/sctp_os_userspace.h  netinet/sctp_process_lock.h  netinet/sctp_timer.h
    netinet/sctp_auth.h      netinet/sctp_crc32.h      netinet/sctp_input.h           netinet/sctp_output.h        netinet/sctp_sha1.h          netinet/sctp_uio.h
    netinet/sctp_bsd_addr.h  netinet/sctp.h            netinet/sctp_lock_userspace.h  netinet/sctp_pcb.h           netinet/sctp_structs.h       netinet/sctputil.h
    netinet/sctp_callout.h   netinet/sctp_header.h     netinet/sctp_os.h              netinet/sctp_peeloff.h       netinet/sctp_sysctl.h        netinet/sctp_var.h
)

list(APPEND usrsctp_netinet6_HEADERS
netinet6/sctp6_var.h
)

list(APPEND usrsctp_HEADERS
    ${usrsctp_root_HEADERS}
    ${usrsctp_netinet_HEADERS}
    ${usrsctp_netinet6_HEADERS}
)

list(APPEND usrsctp_SOURCES
    netinet6/sctp6_usrreq.c  netinet/sctp_callout.c       netinet/sctp_input.c    netinet/sctp_sha1.c          netinet/sctp_userspace.c  user_mbuf.c
    netinet/sctp_asconf.c    netinet/sctp_cc_functions.c  netinet/sctp_output.c   netinet/sctp_ss_functions.c  netinet/sctp_usrreq.c     user_recv_thread.c
    netinet/sctp_auth.c      netinet/sctp_crc32.c         netinet/sctp_pcb.c      netinet/sctp_sysctl.c        netinet/sctputil.c        user_socket.c
    netinet/sctp_bsd_addr.c  netinet/sctp_indata.c        netinet/sctp_peeloff.c  netinet/sctp_timer.c         user_environment.c
)

add_library(usrsctp SHARED ${usrsctp_SOURCES} ${usrsctp_HEADERS})
add_library(usrsctp-static STATIC ${usrsctp_SOURCES} ${usrsctp_HEADERS})

if (WIN32)
    message(STATUS "link library: ws2_32")
    target_link_libraries(usrsctp ws2_32)
    target_link_libraries(usrsctp-static ws2_32)
endif ()

set_target_properties(usrsctp-static PROPERTIES OUTPUT_NAME "usrsctp")
set_target_properties(usrsctp PROPERTIES IMPORT_SUFFIX "_import.lib")
set_target_properties(usrsctp PROPERTIES SOVERSION 1 VERSION 1.0.0)

# SETTINGS FOR UNIX COMPILER
if ("x${CMAKE_C_COMPILER_ID}" STREQUAL "xClang" OR "x${CMAKE_C_COMPILER_ID}" STREQUAL "xGNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wextra -Wall -std=c99 -Wno-unused-parameter")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wextra -Wall -std=c99 -Wno-unused-parameter")
    if (WERROR)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif ()
endif ()

# SETTINGS FOR VISUAL STUDIO COMPILER
if ("x${CMAKE_C_COMPILER_ID}" STREQUAL "xMSVC")
    if (CMAKE_C_FLAGS MATCHES "/W[0-4]")
        STRING(REGEX REPLACE "/W[0-4]" "/W3" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    ELSE ()
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
    endif ()

    if (WERROR)
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX")
    endif()
endif ()

if ("x${CMAKE_C_COMPILER_ID}" STREQUAL "xMSVC90" OR "x${CMAKE_C_COMPILER_ID}" STREQUAL "xMSVC10")
    SET_SOURCE_FILES_PROPERTIES(${usrsctp_SOURCES} PROPERTIES LANGUAGE C)
endif()

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
    SET(CMAKE_INSTALL_LIBDIR lib)
endif ()


#################################################
# INSTALL LIBRARY AND HEADER
#################################################

install(TARGETS usrsctp usrsctp-static DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES usrsctp.h DESTINATION include)
