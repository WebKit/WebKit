/*
    Copyright (C) 2012 Intel Corporation

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

/**
 * @file    ewk_security_origin.h
 * @brief   Security Origin API.
 *
 * Security Origin is the mechanism that defines the access limits of a website.
 * Based on information such as domain, protocol and port, you can grant or not
 * authorization for accessing data and performing certain tasks. Database quota
 * can also be defined based on the security origin.
 *
 * The database related functions will do nothing if WebKit is built without Web
 * SQL Database support.
 */

#ifndef ewk_security_origin_h
#define ewk_security_origin_h

#include <Evas.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Ewk_Security_Origin Ewk_Security_Origin;

/**
 * Returns the protocol of the security origin.
 *
 * @param o security origin object
 *
 * It returns a internal string which should not
 * be modified. The string is guaranteed to be stringshared.
 *
 * @return the protocol scheme
 */
EAPI const char  *ewk_security_origin_protocol_get(Ewk_Security_Origin *o);

/**
 * Returns the host of the security origin.
 *
 * It returns a internal string which should not
 * be modified. The string is guaranteed to be stringshared.
 *
 * @param o security origin object
 *
 * @return the host domain
 */
EAPI const char  *ewk_security_origin_host_get(Ewk_Security_Origin *o);

/**
 * Returns the port of the security origin.
 *
 * @param o security origin object
 *
 * @return the port
 */
EAPI uint32_t     ewk_security_origin_port_get(const Ewk_Security_Origin *o);

/**
 * Retrieves the usage of a database for a security origin.
 *
 * This function won't work if Web SQL Database was not enabled when
 * building WebKit and will just return 0.
 *
 * @param o security origin object
 *
 * @return the usage in bytes
 */
EAPI uint64_t     ewk_security_origin_web_database_usage_get(const Ewk_Security_Origin *o);

/**
 * Retrieves the quota of a database for a security origin.
 *
 * This function won't work if Web SQL Database was not enabled when
 * building WebKit and will just return 0.
 *
 * @param o security origin object
 *
 * @return the quota in bytes
 */
EAPI uint64_t     ewk_security_origin_web_database_quota_get(const Ewk_Security_Origin *o);

/**
 * Sets the database usage quota for a security origin.
 *
 * This function won't work if Web SQL Database was not enabled when
 * building WebKit.
 *
 * @param o security origin object
 * @param quota the usage quota in bytes
 */
EAPI void         ewk_security_origin_web_database_quota_set(const Ewk_Security_Origin *o, uint64_t quota);

/**
 * Release all resources allocated by a security origin object.
 *
 * @param o security origin object
 */
EAPI void         ewk_security_origin_free(Ewk_Security_Origin *o);

#ifdef __cplusplus
}
#endif
#endif // ewk_security_origin_h
