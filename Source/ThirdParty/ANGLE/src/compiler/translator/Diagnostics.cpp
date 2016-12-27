//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/Diagnostics.h"

#include "common/debug.h"
#include "compiler/preprocessor/SourceLocation.h"
#include "compiler/translator/Common.h"
#include "compiler/translator/InfoSink.h"

TDiagnostics::TDiagnostics(TInfoSink& infoSink) :
    mInfoSink(infoSink),
    mNumErrors(0),
    mNumWarnings(0)
{
}

TDiagnostics::~TDiagnostics()
{
}

void TDiagnostics::writeInfo(Severity severity,
                             const pp::SourceLocation& loc,
                             const std::string& reason,
                             const std::string& token,
                             const std::string& extra)
{
    TPrefixType prefix = EPrefixNone;
    switch (severity)
    {
      case PP_ERROR:
        ++mNumErrors;
        prefix = EPrefixError;
        break;
      case PP_WARNING:
        ++mNumWarnings;
        prefix = EPrefixWarning;
        break;
      default:
        UNREACHABLE();
        break;
    }

    TInfoSinkBase& sink = mInfoSink.info;
    /* VC++ format: file(linenum) : error #: 'token' : extrainfo */
    sink.prefix(prefix);
    sink.location(loc.file, loc.line);
    sink << "'" << token <<  "' : " << reason << " " << extra << "\n";
}

void TDiagnostics::error(const TSourceLoc &loc,
                         const char *reason,
                         const char *token,
                         const char *extraInfo)
{
    pp::SourceLocation srcLoc;
    srcLoc.file = loc.first_file;
    srcLoc.line = loc.first_line;
    writeInfo(pp::Diagnostics::PP_ERROR, srcLoc, reason, token, extraInfo);
}

void TDiagnostics::warning(const TSourceLoc &loc,
                           const char *reason,
                           const char *token,
                           const char *extraInfo)
{
    pp::SourceLocation srcLoc;
    srcLoc.file = loc.first_file;
    srcLoc.line = loc.first_line;
    writeInfo(pp::Diagnostics::PP_WARNING, srcLoc, reason, token, extraInfo);
}

void TDiagnostics::print(ID id,
                         const pp::SourceLocation& loc,
                         const std::string& text)
{
    writeInfo(severity(id), loc, message(id), text, "");
}
