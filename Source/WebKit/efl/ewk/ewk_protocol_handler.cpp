/*
    Copyright (C) 2011 ProFUSION embedded systems

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

#include "config.h"
#include "ewk_protocol_handler.h"

#if USE(SOUP)
#include "ewk_protocol_handler_soup.h"
#endif

Eina_Bool ewk_custom_protocol_handler_set(const char** protocols)
{
#if USE(SOUP)
    return ewk_custom_protocol_handler_soup_set(protocols);
#else
    EINA_LOG_CRIT("Not implemented");
    return false;
#endif
}

Eina_Bool ewk_custom_protocol_handler_all_unset()
{
#if USE(SOUP)
    return ewk_custom_protocol_handler_soup_all_unset();
#else
    EINA_LOG_CRIT("Not implemented");
    return false;
#endif
}
