/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef TransferJobManager_H_
#define TransferJobManager_H_

#include "Frame.h"
#include "Timer.h"
#include "TransferJobClient.h"
#include <curl/curl.h>

namespace WebCore {

class TransferJobManager {
public:
    static TransferJobManager* get();
    void add(TransferJob*);
    void cancel(TransferJob*);

    // If true, don't multiplex downloads: download completely one at a time.
    void useSimpleTransfer(bool useSimple);

private:
    TransferJobManager();
    void downloadTimerCallback(Timer<TransferJobManager>*);
    void remove(TransferJob*);

    bool m_useSimple;
    HashSet<TransferJob*>* jobs;
    Timer<TransferJobManager> m_downloadTimer;
    CURLM* curlMultiHandle; // not freed

    // curl filehandles to poll with select
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;

    int maxfd;
    char error_buffer[CURL_ERROR_SIZE];

    // NULL-terminated list of supported protocols
    const char* const* curl_protocols; // not freed
};

}

#endif
