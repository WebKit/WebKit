#
# Copyright (C) 2011-2012 Michael Tuexen
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
# ARE DISCLAIMED.	IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

AM_CPPFLAGS = -I$(srcdir)/../usrsctplib
EXTRA_DIST = Makefile.nmake tsctp.c daytime_server.c discard_server.c echo_server.c client.c rtcweb.c ekr_client.c ekr_server.c ekr_loop.c ekr_loop_offload.c test_libmgmt.c http_client.c

noinst_PROGRAMS = tsctp daytime_server discard_server echo_server client rtcweb ekr_client ekr_server ekr_peer ekr_loop ekr_loop_offload test_libmgmt test_timer http_client
test_libmgmt_SOURCES = test_libmgmt.c
test_libmgmt_LDADD = ../usrsctplib/libusrsctp.la
test_timer_SOURCES = test_timer.c
test_timer_LDADD = ../usrsctplib/libusrsctp.la
tsctp_SOURCES = tsctp.c
tsctp_LDADD = ../usrsctplib/libusrsctp.la
daytime_server_SOURCES = daytime_server.c
daytime_server_LDADD = ../usrsctplib/libusrsctp.la
discard_server_SOURCES = discard_server.c
discard_server_LDADD = ../usrsctplib/libusrsctp.la
echo_server_SOURCES = echo_server.c
echo_server_LDADD = ../usrsctplib/libusrsctp.la
client_SOURCES = client.c
client_LDADD = ../usrsctplib/libusrsctp.la
rtcweb_SOURCES = rtcweb.c
rtcweb_LDADD = ../usrsctplib/libusrsctp.la
ekr_server_SOURCES = ekr_server.c
ekr_server_LDADD = ../usrsctplib/libusrsctp.la
ekr_client_SOURCES = ekr_client.c
ekr_client_LDADD = ../usrsctplib/libusrsctp.la
ekr_peer_SOURCES = ekr_peer.c
ekr_peer_LDADD = ../usrsctplib/libusrsctp.la
ekr_loop_SOURCES = ekr_loop.c
ekr_loop_LDADD = ../usrsctplib/libusrsctp.la
ekr_loop_offload_SOURCES = ekr_loop_offload.c
ekr_loop_offload_LDADD = ../usrsctplib/libusrsctp.la
http_client_SOURCES = http_client.c
http_client_LDADD = ../usrsctplib/libusrsctp.la
