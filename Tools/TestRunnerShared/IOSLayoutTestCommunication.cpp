/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IOSLayoutTestCommunication.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <wtf/Assertions.h>

static int stdinSocket;
static int stdoutSocket;
static int stderrSocket;
static bool isUsingTCP = false;

static int connectToServer(sockaddr_in& serverAddress)
{
    int result = socket(AF_INET, SOCK_STREAM, 0);
    RELEASE_ASSERT(result >= 0);
    RELEASE_ASSERT(connect(result, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) >= 0);
    return result;
}

void setUpIOSLayoutTestCommunication()
{
    char* portFromEnvironment = getenv("PORT");
    if (!portFromEnvironment)
        return;
    int port = atoi(portFromEnvironment);
    RELEASE_ASSERT(port > 0);
    isUsingTCP = true;

    struct hostent* host = gethostbyname("127.0.0.1");
    struct sockaddr_in serverAddress;
    memset((char*) &serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    memcpy(
        (char*)&serverAddress.sin_addr.s_addr,
        (char*)host->h_addr,
        host->h_length);
    serverAddress.sin_port = htons(port);
    
    // This order matches the server side listener in Tools/Scripts/webkitpy/port/simulator_process.py SimulatorProcess._start()
    stdinSocket = connectToServer(serverAddress);
    dup2(stdinSocket, STDIN_FILENO);

    stdoutSocket = connectToServer(serverAddress);
    dup2(stdoutSocket, STDOUT_FILENO);

    stderrSocket = connectToServer(serverAddress);
    dup2(stderrSocket, STDERR_FILENO);
}

void tearDownIOSLayoutTestCommunication()
{
    if (!isUsingTCP)
        return;
    close(stdinSocket);
    close(stdoutSocket);
    close(stderrSocket);
}
