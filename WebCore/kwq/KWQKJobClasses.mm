/*
* Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <jobclasses.h>

namespace KIO {

// class Job ===================================================================

Job::~Job()
{
}


int Job::error()
{
}


const QString & Job::errorText()
{
}


QString Job::errorString()
{
}


void Job::kill(bool quietly=TRUE)
{
}


// class SimpleJob =============================================================

SimpleJob::~SimpleJob()
{
}


// class TransferJob ===========================================================

TransferJob::TransferJob(const KURL &, bool reload=false, bool showProgressInfo=true)
{
}


bool TransferJob::isErrorPage() const
{
}


void TransferJob::addMetaData(const QString &key, const QString &value)
{
}


void TransferJob::kill(bool quietly=TRUE)
{
}


void TransferJob::doLoad()
{
}


} // namespace KIO

