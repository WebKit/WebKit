/*
 * Copyright (C) 2011-2013 Michael Tuexen
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <usrsctp.h>

#define MAX_PACKET_SIZE (1<<16)
#define LINE_LENGTH 80
#define DISCARD_PPID 39

#ifdef _WIN32
static DWORD WINAPI
#else
static void *
#endif
handle_packets(void *arg)
{
#ifdef _WIN32
	SOCKET *fdp;
#else
	int *fdp;
#endif
	ssize_t length;
	char buf[MAX_PACKET_SIZE];

#ifdef _WIN32
	fdp = (SOCKET *)arg;
#else
	fdp = (int *)arg;
#endif
	for (;;) {
#if defined(__NetBSD__)
		pthread_testcancel();
#endif
		length = recv(*fdp, buf, MAX_PACKET_SIZE, 0);
		if (length > 0) {
			usrsctp_conninput(fdp, buf, (size_t)length, 0);
		}
	}
#ifdef _WIN32
	return 0;
#else
	return (NULL);
#endif
}

static int
conn_output(void *addr, void *buffer, size_t length, uint8_t tos, uint8_t set_df)
{
#ifdef _WIN32
	SOCKET *fdp;
#else
	int *fdp;
#endif

#ifdef _WIN32
	fdp = (SOCKET *)addr;
#else
	fdp = (int *)addr;
#endif
#ifdef _WIN32
	if (send(*fdp, buffer, (int)length, 0) == SOCKET_ERROR) {
		return (WSAGetLastError());
#else
	if (send(*fdp, buffer, length, 0) < 0) {
		return (errno);
#endif
	} else {
		return (0);
	}
}

static void
handle_association_change_event(struct sctp_assoc_change *sac)
{
	unsigned int i, n;

	printf("Association change ");
	switch (sac->sac_state) {
	case SCTP_COMM_UP:
		printf("SCTP_COMM_UP");
		break;
	case SCTP_COMM_LOST:
		printf("SCTP_COMM_LOST");
		break;
	case SCTP_RESTART:
		printf("SCTP_RESTART");
		break;
	case SCTP_SHUTDOWN_COMP:
		printf("SCTP_SHUTDOWN_COMP");
		break;
	case SCTP_CANT_STR_ASSOC:
		printf("SCTP_CANT_STR_ASSOC");
		break;
	default:
		printf("UNKNOWN");
		break;
	}
	printf(", streams (in/out) = (%u/%u)",
	       sac->sac_inbound_streams, sac->sac_outbound_streams);
	n = sac->sac_length - sizeof(struct sctp_assoc_change);
	if (((sac->sac_state == SCTP_COMM_UP) ||
	     (sac->sac_state == SCTP_RESTART)) && (n > 0)) {
		printf(", supports");
		for (i = 0; i < n; i++) {
			switch (sac->sac_info[i]) {
			case SCTP_ASSOC_SUPPORTS_PR:
				printf(" PR");
				break;
			case SCTP_ASSOC_SUPPORTS_AUTH:
				printf(" AUTH");
				break;
			case SCTP_ASSOC_SUPPORTS_ASCONF:
				printf(" ASCONF");
				break;
			case SCTP_ASSOC_SUPPORTS_MULTIBUF:
				printf(" MULTIBUF");
				break;
			case SCTP_ASSOC_SUPPORTS_RE_CONFIG:
				printf(" RE-CONFIG");
				break;
			default:
				printf(" UNKNOWN(0x%02x)", sac->sac_info[i]);
				break;
			}
		}
	} else if (((sac->sac_state == SCTP_COMM_LOST) ||
	            (sac->sac_state == SCTP_CANT_STR_ASSOC)) && (n > 0)) {
		printf(", ABORT =");
		for (i = 0; i < n; i++) {
			printf(" 0x%02x", sac->sac_info[i]);
		}
	}
	printf(".\n");
	if ((sac->sac_state == SCTP_CANT_STR_ASSOC) ||
	    (sac->sac_state == SCTP_SHUTDOWN_COMP) ||
	    (sac->sac_state == SCTP_COMM_LOST)) {
		exit(0);
	}
	return;
}

static void
handle_peer_address_change_event(struct sctp_paddr_change *spc)
{
	char addr_buf[INET6_ADDRSTRLEN];
	const char *addr;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sockaddr_conn *sconn;

	switch (spc->spc_aaddr.ss_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)&spc->spc_aaddr;
		addr = inet_ntop(AF_INET, &sin->sin_addr, addr_buf, INET_ADDRSTRLEN);
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)&spc->spc_aaddr;
		addr = inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf, INET6_ADDRSTRLEN);
		break;
	case AF_CONN:
		sconn = (struct sockaddr_conn *)&spc->spc_aaddr;
#ifdef _WIN32
		_snprintf(addr_buf, INET6_ADDRSTRLEN, "%p", sconn->sconn_addr);
#else
		snprintf(addr_buf, INET6_ADDRSTRLEN, "%p", sconn->sconn_addr);
#endif
		addr = addr_buf;
		break;
	default:
#ifdef _WIN32
		_snprintf(addr_buf, INET6_ADDRSTRLEN, "Unknown family %d", spc->spc_aaddr.ss_family);
#else
		snprintf(addr_buf, INET6_ADDRSTRLEN, "Unknown family %d", spc->spc_aaddr.ss_family);
#endif
		addr = addr_buf;
		break;
	}
	printf("Peer address %s is now ", addr);
	switch (spc->spc_state) {
	case SCTP_ADDR_AVAILABLE:
		printf("SCTP_ADDR_AVAILABLE");
		break;
	case SCTP_ADDR_UNREACHABLE:
		printf("SCTP_ADDR_UNREACHABLE");
		break;
	case SCTP_ADDR_REMOVED:
		printf("SCTP_ADDR_REMOVED");
		break;
	case SCTP_ADDR_ADDED:
		printf("SCTP_ADDR_ADDED");
		break;
	case SCTP_ADDR_MADE_PRIM:
		printf("SCTP_ADDR_MADE_PRIM");
		break;
	case SCTP_ADDR_CONFIRMED:
		printf("SCTP_ADDR_CONFIRMED");
		break;
	default:
		printf("UNKNOWN");
		break;
	}
	printf(" (error = 0x%08x).\n", spc->spc_error);
	return;
}

static void
handle_send_failed_event(struct sctp_send_failed_event *ssfe)
{
	size_t i, n;

	if (ssfe->ssfe_flags & SCTP_DATA_UNSENT) {
		printf("Unsent ");
	}
	if (ssfe->ssfe_flags & SCTP_DATA_SENT) {
		printf("Sent ");
	}
	if (ssfe->ssfe_flags & ~(SCTP_DATA_SENT | SCTP_DATA_UNSENT)) {
		printf("(flags = %x) ", ssfe->ssfe_flags);
	}
	printf("message with PPID = %u, SID = %u, flags: 0x%04x due to error = 0x%08x",
	       ntohl(ssfe->ssfe_info.snd_ppid), ssfe->ssfe_info.snd_sid,
	       ssfe->ssfe_info.snd_flags, ssfe->ssfe_error);
	n = ssfe->ssfe_length - sizeof(struct sctp_send_failed_event);
	for (i = 0; i < n; i++) {
		printf(" 0x%02x", ssfe->ssfe_data[i]);
	}
	printf(".\n");
	return;
}

static void
handle_notification(union sctp_notification *notif, size_t n)
{
	if (notif->sn_header.sn_length != (uint32_t)n) {
		return;
	}
	switch (notif->sn_header.sn_type) {
	case SCTP_ASSOC_CHANGE:
		handle_association_change_event(&(notif->sn_assoc_change));
		break;
	case SCTP_PEER_ADDR_CHANGE:
		handle_peer_address_change_event(&(notif->sn_paddr_change));
		break;
	case SCTP_REMOTE_ERROR:
		break;
	case SCTP_SHUTDOWN_EVENT:
		break;
	case SCTP_ADAPTATION_INDICATION:
		break;
	case SCTP_PARTIAL_DELIVERY_EVENT:
		break;
	case SCTP_AUTHENTICATION_EVENT:
		break;
	case SCTP_SENDER_DRY_EVENT:
		break;
	case SCTP_NOTIFICATIONS_STOPPED_EVENT:
		break;
	case SCTP_SEND_FAILED_EVENT:
		handle_send_failed_event(&(notif->sn_send_failed_event));
		break;
	case SCTP_STREAM_RESET_EVENT:
		break;
	case SCTP_ASSOC_RESET_EVENT:
		break;
	case SCTP_STREAM_CHANGE_EVENT:
		break;
	default:
		break;
	}
}

static int
receive_cb(struct socket *sock, union sctp_sockstore addr, void *data,
           size_t datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{
	if (data) {
		if (flags & MSG_NOTIFICATION) {
			handle_notification((union sctp_notification *)data, datalen);
		} else {
			printf("Msg of length %d received via %p:%u on stream %u with SSN %u and TSN %u, PPID %u, context %u.\n",
			       (int)datalen,
			       addr.sconn.sconn_addr,
			       ntohs(addr.sconn.sconn_port),
			       rcv.rcv_sid,
			       rcv.rcv_ssn,
			       rcv.rcv_tsn,
			       ntohl(rcv.rcv_ppid),
			       rcv.rcv_context);
		}
		free(data);
	} else {
		usrsctp_deregister_address(ulp_info);
		usrsctp_close(sock);
	}
	return (1);
}

void
debug_printf(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_in sin;
	struct sockaddr_conn sconn;
	struct sctp_event event;
	uint16_t event_types[] = {SCTP_ASSOC_CHANGE,
	                          SCTP_PEER_ADDR_CHANGE,
	                          SCTP_SEND_FAILED_EVENT};
	unsigned int i;
#ifdef _WIN32
	SOCKET fd;
#else
	int fd;
#endif
	struct socket *s;
#ifdef _WIN32
	HANDLE tid;
#else
	pthread_t tid;
#endif
	struct sctp_sndinfo sndinfo;
	char line[LINE_LENGTH];
#ifdef _WIN32
	WSADATA wsaData;
#endif

	if (argc < 4) {
		printf("error: this program requires 4 arguments!\n");
		exit(EXIT_FAILURE);
	}

#ifdef _WIN32
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
		printf("WSAStartup failed\n");
		exit(EXIT_FAILURE);
	}
#endif
	usrsctp_init(0, conn_output, debug_printf);
	/* set up a connected UDP socket */
#ifdef _WIN32
	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		printf("socket() failed with error: %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
#else
	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
#endif
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
	sin.sin_len = sizeof(struct sockaddr_in);
#endif
	sin.sin_port = htons(atoi(argv[2]));
	if (!inet_pton(AF_INET, argv[1], &sin.sin_addr.s_addr)){
		printf("error: invalid address\n");
		exit(EXIT_FAILURE);
	}
#ifdef _WIN32
	if (bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		printf("bind() failed with error: %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
#else
	if (bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
#endif
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
	sin.sin_len = sizeof(struct sockaddr_in);
#endif
	sin.sin_port = htons(atoi(argv[4]));
	if (!inet_pton(AF_INET, argv[3], &sin.sin_addr.s_addr)){
		printf("error: invalid address\n");
		exit(EXIT_FAILURE);
	}
#ifdef _WIN32
	if (connect(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		printf("connect() failed with error: %d\n", WSAGetLastError());
		exit(EXIT_FAILURE);
	}
#else
	if (connect(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
		perror("connect");
		exit(EXIT_FAILURE);
	}
#endif
#ifdef _WIN32
	tid = CreateThread(NULL, 0, &handle_packets, (void *)&fd, 0, NULL);
#else
	pthread_create(&tid, NULL, &handle_packets, (void *)&fd);
#endif
#ifdef SCTP_DEBUG
	usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_NONE);
#endif
	usrsctp_register_address((void *)&fd);
	usrsctp_sysctl_set_sctp_ecn_enable(0);
	if ((s = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, receive_cb, NULL, 0, &fd)) == NULL) {
		perror("usrsctp_socket");
	}
	/* Enable the events of interest. */
	if (usrsctp_set_non_blocking(s, 1) < 0) {
		perror("usrsctp_set_non_blocking");
	}
	memset(&event, 0, sizeof(event));
	event.se_assoc_id = SCTP_ALL_ASSOC;
	event.se_on = 1;
	for (i = 0; i < sizeof(event_types)/sizeof(uint16_t); i++) {
		event.se_type = event_types[i];
		if (usrsctp_setsockopt(s, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event)) < 0) {
			perror("setsockopt SCTP_EVENT");
		}
	}
	memset(&sconn, 0, sizeof(struct sockaddr_conn));
	sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
	sconn.sconn_port = htons(atoi(argv[5]));
	sconn.sconn_addr = &fd;
	if (usrsctp_bind(s, (struct sockaddr *)&sconn, sizeof(struct sockaddr_conn)) < 0) {
		perror("usrsctp_bind");
	}
	memset(&sconn, 0, sizeof(struct sockaddr_conn));
	sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
	sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
	sconn.sconn_port = htons(atoi(argv[6]));
	sconn.sconn_addr = &fd;
	if (usrsctp_connect(s, (struct sockaddr *)&sconn, sizeof(struct sockaddr_conn)) < 0) {
		perror("usrsctp_connect");
	}
	for (;;) {
#ifdef _WIN32
		if (gets_s(line, LINE_LENGTH) == NULL) {
#else
		if (fgets(line, LINE_LENGTH, stdin) == NULL) {
#endif
			if (usrsctp_shutdown(s, SHUT_WR) < 0) {
				perror("usrsctp_shutdown");
			}
			while (usrsctp_finish() != 0) {
#ifdef _WIN32
				Sleep(1000);
#else
				sleep(1);
#endif
			}
			break;
		}
		sndinfo.snd_sid = 1;
		sndinfo.snd_flags = 0;
		sndinfo.snd_ppid = htonl(DISCARD_PPID);
		sndinfo.snd_context = 0;
		sndinfo.snd_assoc_id = 0;
		if (usrsctp_sendv(s, line, strlen(line), NULL, 0, (void *)&sndinfo,
	                          (socklen_t)sizeof(struct sctp_sndinfo), SCTP_SENDV_SNDINFO, 0) < 0) {
			perror("usrsctp_sendv");
		}
	}
	while (usrsctp_finish() != 0) {
#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}
#ifdef _WIN32
	TerminateThread(tid, 0);
	WaitForSingleObject(tid, INFINITE);
	if (closesocket(fd) == SOCKET_ERROR) {
		printf("closesocket() failed with error: %d\n", WSAGetLastError());
	}
	WSACleanup();
#else
	pthread_cancel(tid);
	pthread_join(tid, NULL);
	if (close(fd) < 0) {
		perror("close");
	}
#endif
	return (0);
}
