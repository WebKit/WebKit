/*
 *  Copyright (C) 2021 Igalia S.L.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <dlfcn.h>
#include <signal.h>

typedef int (*real_sigaction_t)(int, const struct sigaction*, struct sigaction*);

int realSigaction(int signum, const struct sigaction* act, struct sigaction* oldact)
{
    return ((real_sigaction_t) dlsym(RTLD_NEXT, "sigaction"))(signum, act, oldact);
}

int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact)
{
    if (signum == SIGINT || signum == SIGTERM || signum == SIGHUP)
        return 0;

    return realSigaction(signum, act, oldact);
}
