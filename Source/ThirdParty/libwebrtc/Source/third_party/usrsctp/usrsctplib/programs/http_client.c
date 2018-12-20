/*
 * Copyright (C) 2016 Felix Weinrank
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

/*
 * Usage: http_client remote_addr remote_port [local_port] [local_encaps_port] [remote_encaps_port] [uri]
 * 
 * Example
 * Client: $ ./http_client 212.201.121.100 80 0 9899 9899 /cgi-bin/he
 *           ./http_client 2a02:c6a0:4015:10::100 80 0 9899 9899 /cgi-bin/he
 */

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <io.h>
#endif
#include <usrsctp.h>

int done = 0;
static const char *request_prefix = "GET";
static const char *request_postfix = "HTTP/1.0\r\nUser-agent: libusrsctp\r\nConnection: close\r\n\r\n";
char request[512];

#ifdef _WIN32
typedef char* caddr_t;
#endif

static int
receive_cb(struct socket *sock, union sctp_sockstore addr, void *data,
           size_t datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{
	if (data == NULL) {
		done = 1;
		usrsctp_close(sock);
	} else {
#ifdef _WIN32
		_write(_fileno(stdout), data, (unsigned int)datalen);
#else
		if (write(fileno(stdout), data, datalen) < 0) {
			perror("write");
		}
#endif
		free(data);
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
	struct socket *sock;
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	struct sctp_udpencaps encaps;
	struct sctp_sndinfo sndinfo;
	int result;

    if (argc < 3) {
        printf("Usage: http_client remote_addr remote_port [local_port] [local_encaps_port] [remote_encaps_port] [uri]\n");
        return(EXIT_FAILURE);
    }

	result = 0;
	if (argc > 4) {
		usrsctp_init(atoi(argv[4]), NULL, debug_printf);
	} else {
		usrsctp_init(9899, NULL, debug_printf);
	}

#ifdef SCTP_DEBUG
	usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
#endif

	usrsctp_sysctl_set_sctp_blackhole(2);

	if ((sock = usrsctp_socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP, receive_cb, NULL, 0, NULL)) == NULL) {
		perror("usrsctp_socket");
		result = 1;
		goto out;
	}

	if (argc > 3) {
		memset((void *)&addr6, 0, sizeof(struct sockaddr_in6));
#ifdef HAVE_SIN6_LEN
		addr6.sin6_len = sizeof(struct sockaddr_in6);
#endif
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(atoi(argv[3]));
		addr6.sin6_addr = in6addr_any;
		if (usrsctp_bind(sock, (struct sockaddr *)&addr6, sizeof(struct sockaddr_in6)) < 0) {
			perror("bind");
			usrsctp_close(sock);
			result = 2;
			goto out;
		}
	}

	if (argc > 5) {
		memset(&encaps, 0, sizeof(struct sctp_udpencaps));
		encaps.sue_address.ss_family = AF_INET6;
		encaps.sue_port = htons(atoi(argv[5]));
		if (usrsctp_setsockopt(sock, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT, (const void *)&encaps, (socklen_t)sizeof(struct sctp_udpencaps)) < 0) {
			perror("setsockopt");
			usrsctp_close(sock);
			result = 3;
			goto out;
		}
	}

	if (argc > 6) {
#ifdef _WIN32
		_snprintf(request, sizeof(request), "%s %s %s", request_prefix, argv[6], request_postfix);
#else
		snprintf(request, sizeof(request), "%s %s %s", request_prefix, argv[6], request_postfix);
#endif
	} else {
#ifdef _WIN32
		_snprintf(request, sizeof(request), "%s %s %s", request_prefix, "/", request_postfix);
#else
		snprintf(request, sizeof(request), "%s %s %s", request_prefix, "/", request_postfix);
#endif
	}

	printf("\nHTTP request:\n%s\n", request);
	printf("\nHTTP response:\n");

	memset((void *)&addr4, 0, sizeof(struct sockaddr_in));
	memset((void *)&addr6, 0, sizeof(struct sockaddr_in6));
#ifdef HAVE_SIN_LEN
	addr4.sin_len = sizeof(struct sockaddr_in);
#endif
#ifdef HAVE_SIN6_LEN
	addr6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	addr4.sin_family = AF_INET;
	addr6.sin6_family = AF_INET6;
	addr4.sin_port = htons(atoi(argv[2]));
	addr6.sin6_port = htons(atoi(argv[2]));
	if (inet_pton(AF_INET6, argv[1], &addr6.sin6_addr) == 1) {
		if (usrsctp_connect(sock, (struct sockaddr *)&addr6, sizeof(struct sockaddr_in6)) < 0) {
			perror("usrsctp_connect");
			usrsctp_close(sock);
			result = 4;
			goto out;
		}
	} else if (inet_pton(AF_INET, argv[1], &addr4.sin_addr) == 1) {
		if (usrsctp_connect(sock, (struct sockaddr *)&addr4, sizeof(struct sockaddr_in)) < 0) {
			perror("usrsctp_connect");
			usrsctp_close(sock);
			result = 5;
			goto out;
		}
	} else {
		printf("Illegal destination address\n");
		usrsctp_close(sock);
		result = 6;
		goto out;
	}

	memset(&sndinfo, 0, sizeof(struct sctp_sndinfo));
	sndinfo.snd_ppid = htonl(63); /* PPID for HTTP/SCTP */
	/* send GET request */
	if (usrsctp_sendv(sock, request, strlen(request), NULL, 0, &sndinfo, sizeof(struct sctp_sndinfo), SCTP_SENDV_SNDINFO, 0) < 0) {
		perror("usrsctp_sendv");
		usrsctp_close(sock);
		result = 6;
		goto out;
	}

	while (!done) {
#ifdef _WIN32
		Sleep(1*1000);
#else
		sleep(1);
#endif
	}
out:
	while (usrsctp_finish() != 0) {
#ifdef _WIN32
		Sleep(1000);
#else
		sleep(1);
#endif
	}
	return (result);
}
