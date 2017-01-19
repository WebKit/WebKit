/*
 * err.c
 *
 * error status reporting functions
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *
 * Copyright(c) 2001-2006 Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include "err.h"


/*  srtp_err_level reflects the level of errors that are reported  */

srtp_err_reporting_level_t srtp_err_level = srtp_err_level_none;

/* srtp_err_file is the FILE to which errors are reported */

static FILE *srtp_err_file = NULL;

srtp_err_status_t srtp_err_reporting_init (const char *ident)
{

    /*
     * Believe it or not, openlog doesn't return an error on failure.
     * But then, neither does the syslog() call...
     */

#ifdef ERR_REPORTING_STDOUT
    srtp_err_file = stdout;
#elif defined(USE_ERR_REPORTING_FILE)
    /* open file for error reporting */
    srtp_err_file = fopen(ERR_REPORTING_FILE, "w");
    if (srtp_err_file == NULL) {
        return srtp_err_status_init_fail;
    }
#endif

    return srtp_err_status_ok;
}

void srtp_err_report (int priority, const char *format, ...)
{
    va_list args;

    if (priority <= srtp_err_level) {

        va_start(args, format);
        if (srtp_err_file != NULL) {
            vfprintf(srtp_err_file, format, args);
            /*      fprintf(srtp_err_file, "\n"); */
        }
        va_end(args);
    }
}

void srtp_err_reporting_set_level (srtp_err_reporting_level_t lvl)
{
    srtp_err_level = lvl;
}
