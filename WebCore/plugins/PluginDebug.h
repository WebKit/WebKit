/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef PLUGIN_DEBUG_H__
#define PLUGIN_DEBUG_H__

#include "Logging.h"
#include "npruntime_internal.h"

static const char* errorStrings[] = {
    "No errors occurred.", /* NPERR_NO_ERROR */
    "Error with no specific error code occurred.", /* NPERR_GENERIC_ERROR */
    "Invalid instance passed to the plug-in.", /* NPERR_INVALID_INSTANCE_ERROR */
    "Function table invalid.", /* NPERR_INVALID_FUNCTABLE_ERROR */
    "Loading of plug-in failed.", /* NPERR_MODULE_LOAD_FAILED_ERROR */
    "Memory allocation failed.", /* NPERR_OUT_OF_MEMORY_ERROR */
    "Plug-in missing or invalid.", /* NPERR_INVALID_PLUGIN_ERROR */
    "Plug-in directory missing or invalid.", /* NPERR_INVALID_PLUGIN_DIR_ERROR */
    "Versions of plug-in and Communicator do not match.", /* NPERR_INCOMPATIBLE_VERSION_ERROR */
    "Parameter missing or invalid.", /* NPERR_INVALID_PARAM */
    "URL missing or invalid.", /* NPERR_INVALID_URL */
    "File missing or invalid.", /* NPERR_FILE_NOT_FOUND */
    "Stream contains no data.", /* NPERR_NO_DATA */
    "Seekable stream expected.", /* NPERR_STREAM_NOT_SEEKABLE */
    "Unknown error code"
};

#define LOG_NPERROR(err) if (err != NPERR_NO_ERROR) LOG_VERBOSE(Plugin, "%s\n", errorStrings[err])
#define LOG_PLUGIN_NET_ERROR() LOG_VERBOSE(Plugin, "Stream failed due to problems with network, disk I/O, lack of memory, or other problems.\n")

#endif
