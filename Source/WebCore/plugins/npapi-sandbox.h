/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

/*!
 * @header WebKit Mac Sandbox
 *
 * This header provides an *EXPERIMENTAL* API for NPAPI plug-ins that wish
 * to reduce their runtime privileges. When entering the sandbox, adopting
 * plug-ins specify a whitelist of files they wish to have available for
 * reading and writing. Plug-ins that need to have access to arbitrary files,
 * such as to enable user-initiated file uploads or downloads, can invoke
 * native Cocoa APIs, which will automatically grant permissions to the plug-in.
 *
 *
 * Security characteristics
 *
 * Sandboxed plug-in's local file access is restricted to the union of:
 *
 * 1. The file whitelist specified by the plug-in when entering the sandbox.
 * 2. Any files obtained at runtime through the secure Open and Save dialogs.
 * 3. Any files that were open before entering the sandbox.
 *
 * Implementation may additionally choose to restrict the creation of IPC
 * channels or the use of other security-sensitive system resources, such as
 * the ability to spawn additional processes. However, any such resources
 * acquired before entry into the sandbox are guaranteed to remain available
 * within it. For example, plug-ins are free to spawn a trusted broker process
 * before entering the sandbox and to establish an IPC channel with it. The
 * channel will remain available within the sandbox even if the implementation
 * is highly restrictive and allows neither process creation nor IPC channel
 * establishment.
 */


#ifndef npapi_sandbox_h
#define npapi_sandbox_h

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @constant WKNVSandboxFunctions
 * Use this constant with NPN_GetValue to get a pointer to WKNSandboxFunctions
 * structure, which contains pointers to sandboxing functions.
 */
#define WKNVSandboxFunctions 74659

/*!
 * Version of WKNSandboxFunctions structure that is returned by NPN_GetValue.
 */
#define WKNVSandboxFunctionsVersionCurrent 1

/*!
 * @function WKN_EnterSandbox
 * Requests that the plug-in instance be placed in a sandbox.
 *
 * @param readOnlyPaths
 * NULL-terminated C array of paths to files and directories that the plug-in
 * wishes to have available in the sandbox for read-only access. Any
 * directories in this array will automatically extend the sandbox to encompass
 * all their files and subdirectories, meaning that they will be available
 * through OS level file access functions for reading.
 *
 * @param readWritePaths
 * NULL-terminated C array of paths to files and directories that the plug-in
 * wishes to have available in the sandbox for both reading and writing.
 * Typically, this array will include paths to the plug-in's local caches, a
 * directory for temporary files, and any configuration files which are not
 * security sensitive, in that they are not consulted when determining the
 * lists of paths to pass to this function during plug-in instantiation.
 * Any directories in this array will automatically extend the sandbox to
 * encompass all their files and subdirectories, meaning that they will be
 * available through OS level file access functions for reading and writing.
 *
 * @result
 * Places the plug-in instance in a sandbox. The sandbox allows read-only access
 * to paths specified by `readOnlyPaths` and read-write access to
 * `readWritePaths`. If the same path appears in both `readOnlyPaths` and
 * `readWritePaths`, access to that path will be read-only. No other filesystem
 * access is available except as expressly permitted by the user through
 * NSOpenPanel and NSSavePanel invocations.  Returns NPERR_NO_ERROR when the
 * navigator has successfully placed the plug-in in a sandbox and
 * NPERR_GENERIC_ERROR if the the plug-in instance was already placed in a sandbox
 * with a prior call to this function. If entering sandbox fails for any other reason,
 * the process is terminated.
 * Note that either or both `readOnlyPaths` and `readWritePaths` may be NULL.
 *
 * @discussion
 * This function should be called as early as possible during plug-in
 * instantiation and ALWAYS before any untrusted code has run. Generally, the
 * only things that the plug-in should do before entering the sandbox are to
 * determine the paths to pass in as `readOnlyPaths` and `readWritePaths`, and
 * to establish any IPC channels to be used from the sandbox. In cases where
 * the plug-in must parse its configuration files to determine any needed local
 * resources (such as files to preload), it should do so and then immediately
 * call this function. If configuration files do not influence the paths that
 * the plug-in will pass as part of `readOnlyPaths` and `readWritePaths`, the
 * plug-in should enter the sandbox first and only then process configuration
 * files and deal with normal startup tasks.
 *
 * Very close attention must be paid to weeding out security-sensitive files
 * from the `readWritePaths` list. If the plug-in instance reads a configuration
 * file at startup to determine which additional files it will place in the
 * `readWritePaths` list to this call, then that configuration file MUST NOT be
 * in the `readWritePaths` itself. Otherwise, should the plug-in become
 * compromised, it can trivially escape its sandbox the next time it is
 * instantiated by writing arbitrary paths (or just "/") into this writable
 * configuration file.
 *
 * Note that after a plug-in instance enters the sandbox, any native calls that
 * it makes which refer to arbitrary paths on disk  will only work if the paths are
 * available within the sandbox, either statically through `readOnlyPaths` and
 * `readWritePaths`, or dynamically through Cocoa APIs.
 * However, NPAPI calls (such as e.g. NPN_PostURL with the file parameter set to TRUE)
 * can access files that navigator policies allow, which is usually an entirely
 * different set for remote and for local Web pages.
 */
typedef NPError (*WKN_EnterSandboxProcPtr)(const char *readOnlyPaths[], const char *readWritePaths[]);

/*!
 * @function WKN_FileStopAccessing
 * Requests that the navigator revoke the plug-in's access to the given path.
 *
 * @param path
 * Required. A path that was previously returned from NSOpenPanel or NSSavePanel.
 *
 * @result
 * Returns  NPERR_NO_ERROR, or NPERR_GENERIC_ERROR if the requesting plug-in
 * instance is not in a sandbox.
 *
 * @discussion
 * Whenever file access is provided to a plug-in instance through a
 * Cocoa API, navigator should be notfied when it is no longer needed.
 * This will cause subsequent open(2) calls on any of the revoked files to fail,
 * but by design no attempt is made to invalidate existing file descriptors.
 * plug-in writers are strongly encouraged to keep files open for as short a period
 * of time as possible, and to always call this function when objects representing
 * the returned files go out of scope or are otherwise destroyed.
 */
typedef NPError (*WKN_FileStopAccessingProcPtr)(const char* path);

typedef struct _WKNSandboxFunctions {
    uint16_t size;
    uint16_t version;
    
    WKN_EnterSandboxProcPtr enterSandbox;
    WKN_FileStopAccessingProcPtr fileStopAccessing;
} WKNSandboxFunctions;

#ifdef __cplusplus
}
#endif

#endif // npapi_sandbox_h
