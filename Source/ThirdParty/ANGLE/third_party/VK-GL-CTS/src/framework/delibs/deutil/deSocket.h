#ifndef _DESOCKET_H
#define _DESOCKET_H
/*-------------------------------------------------------------------------
 * drawElements Utility Library
 * ----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Socket abstraction.
 *
 * Socket API is thread-safe except to:
 *  - listen()
 *  - connect()
 *  - destroy()
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

DE_BEGIN_EXTERN_C

/* Socket types. */
typedef struct deSocket_s deSocket;
typedef struct deSocketAddress_s deSocketAddress;

typedef enum deSocketFamily_e
{
    DE_SOCKETFAMILY_INET4 = 0,
    DE_SOCKETFAMILY_INET6,

    DE_SOCKETFAMILY_LAST
} deSocketFamily;

typedef enum deSocketType_e
{
    DE_SOCKETTYPE_STREAM = 0,
    DE_SOCKETTYPE_DATAGRAM,

    DE_SOCKETTYPE_LAST
} deSocketType;

typedef enum deSocketProtocol_e
{
    DE_SOCKETPROTOCOL_TCP = 0,
    DE_SOCKETPROTOCOL_UDP,

    DE_SOCKETPROTOCOL_LAST
} deSocketProtocol;

typedef enum deSocketFlag_e
{
    DE_SOCKET_KEEPALIVE     = (1 << 0),
    DE_SOCKET_NODELAY       = (1 << 1),
    DE_SOCKET_NONBLOCKING   = (1 << 2),
    DE_SOCKET_CLOSE_ON_EXEC = (1 << 3)
} deSocketFlag;

/* \todo [2012-07-09 pyry] Separate closed bits for send and receive channels. */

typedef enum deSocketState_e
{
    DE_SOCKETSTATE_CLOSED       = 0,
    DE_SOCKETSTATE_CONNECTED    = 1,
    DE_SOCKETSTATE_LISTENING    = 2,
    DE_SOCKETSTATE_DISCONNECTED = 3,

    DE_SOCKETSTATE_LAST
} deSocketState;

typedef enum deSocketResult_e
{
    DE_SOCKETRESULT_SUCCESS               = 0,
    DE_SOCKETRESULT_WOULD_BLOCK           = 1,
    DE_SOCKETRESULT_CONNECTION_CLOSED     = 2,
    DE_SOCKETRESULT_CONNECTION_TERMINATED = 3,
    DE_SOCKETRESULT_ERROR                 = 4,

    DE_SOCKETRESULT_LAST
} deSocketResult;

typedef enum deSocketChannel_e
{
    DE_SOCKETCHANNEL_RECEIVE = (1 << 0),
    DE_SOCKETCHANNEL_SEND    = (1 << 1),

    DE_SOCKETCHANNEL_BOTH = DE_SOCKETCHANNEL_RECEIVE | DE_SOCKETCHANNEL_SEND
} deSocketChannel;

/* Socket API, similar to Berkeley sockets. */

deSocketAddress *deSocketAddress_create(void);
void deSocketAddress_destroy(deSocketAddress *address);

bool deSocketAddress_setFamily(deSocketAddress *address, deSocketFamily family);
deSocketFamily deSocketAddress_getFamily(const deSocketAddress *address);
bool deSocketAddress_setType(deSocketAddress *address, deSocketType type);
deSocketType deSocketAddress_getType(const deSocketAddress *address);
bool deSocketAddress_setProtocol(deSocketAddress *address, deSocketProtocol protocol);
deSocketProtocol deSocketAddress_getProtocol(const deSocketAddress *address);
bool deSocketAddress_setPort(deSocketAddress *address, int port);
int deSocketAddress_getPort(const deSocketAddress *address);
bool deSocketAddress_setHost(deSocketAddress *address, const char *host);
const char *deSocketAddress_getHost(const deSocketAddress *address);

deSocket *deSocket_create(void);
void deSocket_destroy(deSocket *socket);

deSocketState deSocket_getState(const deSocket *socket);
uint32_t deSocket_getOpenChannels(const deSocket *socket);

bool deSocket_setFlags(deSocket *socket, uint32_t flags);

bool deSocket_listen(deSocket *socket, const deSocketAddress *address);
deSocket *deSocket_accept(deSocket *socket, deSocketAddress *clientAddress);

bool deSocket_connect(deSocket *socket, const deSocketAddress *address);

bool deSocket_shutdown(deSocket *socket, uint32_t channels);
bool deSocket_close(deSocket *socket);

deSocketResult deSocket_send(deSocket *socket, const void *buf, size_t bufSize, size_t *numSent);
deSocketResult deSocket_receive(deSocket *socket, void *buf, size_t bufSize, size_t *numReceived);

/* Utilities. */

const char *deGetSocketFamilyName(deSocketFamily family);
const char *deGetSocketResultName(deSocketResult result);

DE_END_EXTERN_C

#endif /* _DESOCKET_H */
