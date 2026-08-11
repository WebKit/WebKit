/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * Copyright (C) 2017 NAVER Corp.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */

#pragma once

#include "CurlContext.h"

namespace WebCore {

class CurlRequestSchedulerClient {
public:
    virtual void retain() = 0;
    virtual void release() = 0;

    virtual CURL* handle() = 0;
    virtual CURL* setupTransfer() = 0;
    virtual void didCompleteTransfer(CURLcode) = 0;
    virtual void didCancelTransfer() = 0;

protected:
    ~CurlRequestSchedulerClient() { }
};

} // namespace WebCore
