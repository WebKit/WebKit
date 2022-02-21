/*
 * Copyright 2021 Collabora Ltd.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

// This file is a copy of flatpak-syscalls-private.h, reformatted a bit to placate WebKit's style checker.
//
// Upstream is here:
// https://github.com/flatpak/flatpak/blob/26b12484eb8a6219b9e7aa287b298a894b2f34ca/common/flatpak-syscalls-private.h

#pragma once

#include <sys/syscall.h>

#if defined(_MIPS_SIM)
# if _MIPS_SIM == _MIPS_SIM_ABI32
#   define FLATPAK_MISSING_SYSCALL_BASE 4000
# elif _MIPS_SIM == _MIPS_SIM_ABI64
#   define FLATPAK_MISSING_SYSCALL_BASE 5000
# elif _MIPS_SIM == _MIPS_SIM_NABI32
#   define FLATPAK_MISSING_SYSCALL_BASE 6000
# else
#   error "Unknown MIPS ABI"
# endif
#endif

#if defined(__ia64__)
# define FLATPAK_MISSING_SYSCALL_BASE 1024
#endif

#if defined(__alpha__)
# define FLATPAK_MISSING_SYSCALL_BASE 110
#endif

#if defined(__x86_64__) && defined(__ILP32__)
# define FLATPAK_MISSING_SYSCALL_BASE 0x40000000
#endif

// FLATPAK_MISSING_SYSCALL_BASE:
//
// Number to add to the syscall numbers of recently-added syscalls
// to get the appropriate syscall for the current ABI.
#ifndef FLATPAK_MISSING_SYSCALL_BASE
# define FLATPAK_MISSING_SYSCALL_BASE 0
#endif

#ifndef __NR_open_tree
# define __NR_open_tree (FLATPAK_MISSING_SYSCALL_BASE + 428)
#endif
#ifndef __SNR_open_tree
# define __SNR_open_tree __NR_open_tree
#endif

#ifndef __NR_move_mount
# define __NR_move_mount (FLATPAK_MISSING_SYSCALL_BASE + 429)
#endif
#ifndef __SNR_move_mount
# define __SNR_move_mount __NR_move_mount
#endif

#ifndef __NR_fsopen
# define __NR_fsopen (FLATPAK_MISSING_SYSCALL_BASE + 430)
#endif
#ifndef __SNR_fsopen
# define __SNR_fsopen __NR_fsopen
#endif

#ifndef __NR_fsconfig
# define __NR_fsconfig (FLATPAK_MISSING_SYSCALL_BASE + 431)
#endif
#ifndef __SNR_fsconfig
# define __SNR_fsconfig __NR_fsconfig
#endif

#ifndef __NR_fsmount
# define __NR_fsmount (FLATPAK_MISSING_SYSCALL_BASE + 432)
#endif
#ifndef __SNR_fsmount
# define __SNR_fsmount __NR_fsmount
#endif

#ifndef __NR_fspick
# define __NR_fspick (FLATPAK_MISSING_SYSCALL_BASE + 433)
#endif
#ifndef __SNR_fspick
# define __SNR_fspick __NR_fspick
#endif

#ifndef __NR_pidfd_open
# define __NR_pidfd_open (FLATPAK_MISSING_SYSCALL_BASE + 434)
#endif
#ifndef __SNR_pidfd_open
# define __SNR_pidfd_open __NR_pidfd_open
#endif

#ifndef __NR_clone3
# define __NR_clone3 (FLATPAK_MISSING_SYSCALL_BASE + 435)
#endif
#ifndef __SNR_clone3
# define __SNR_clone3 __NR_clone3
#endif

#ifndef __NR_close_range
# define __NR_close_range (FLATPAK_MISSING_SYSCALL_BASE + 436)
#endif
#ifndef __SNR_close_range
# define __SNR_close_range __NR_close_range
#endif

#ifndef __NR_openat2
# define __NR_openat2 (FLATPAK_MISSING_SYSCALL_BASE + 437)
#endif
#ifndef __SNR_openat2
# define __SNR_openat2 __NR_openat2
#endif

#ifndef __NR_pidfd_getfd
# define __NR_pidfd_getfd (FLATPAK_MISSING_SYSCALL_BASE + 438)
#endif
#ifndef __SNR_pidfd_getfd
# define __SNR_pidfd_getfd __NR_pidfd_getfd
#endif

#ifndef __NR_faccessat2
# define __NR_faccessat2 (FLATPAK_MISSING_SYSCALL_BASE + 439)
#endif
#ifndef __SNR_faccessat2
# define __SNR_faccessat2 __NR_faccessat2
#endif

#ifndef __NR_process_madvise
# define __NR_process_madvise (FLATPAK_MISSING_SYSCALL_BASE + 440)
#endif
#ifndef __SNR_process_madvise
# define __SNR_process_madvise __NR_process_madvise
#endif

#ifndef __NR_epoll_pwait2
# define __NR_epoll_pwait2 (FLATPAK_MISSING_SYSCALL_BASE + 441)
#endif
#ifndef __SNR_epoll_pwait2
# define __SNR_epoll_pwait2 __NR_epoll_pwait2
#endif

#ifndef __NR_mount_setattr
# define __NR_mount_setattr (FLATPAK_MISSING_SYSCALL_BASE + 442)
#endif
#ifndef __SNR_mount_setattr
# define __SNR_mount_setattr __NR_mount_setattr
#endif

#ifndef __NR_quotactl_fd
# define __NR_quotactl_fd (FLATPAK_MISSING_SYSCALL_BASE + 443)
#endif
#ifndef __SNR_quotactl_fd
# define __SNR_quotactl_fd __NR_quotactl_fd
#endif

#ifndef __NR_landlock_create_ruleset
# define __NR_landlock_create_ruleset (FLATPAK_MISSING_SYSCALL_BASE + 444)
#endif
#ifndef __SNR_landlock_create_ruleset
# define __SNR_landlock_create_ruleset __NR_landlock_create_ruleset
#endif

#ifndef __NR_landlock_add_rule
# define __NR_landlock_add_rule (FLATPAK_MISSING_SYSCALL_BASE + 445)
#endif
#ifndef __SNR_landlock_add_rule
# define __SNR_landlock_add_rule __NR_landlock_add_rule
#endif

#ifndef __NR_landlock_restrict_self
# define __NR_landlock_restrict_self (FLATPAK_MISSING_SYSCALL_BASE + 446)
#endif
#ifndef __SNR_landlock_restrict_self
# define __SNR_landlock_restrict_self __NR_landlock_restrict_self
#endif

#ifndef __NR_memfd_secret
# define __NR_memfd_secret (FLATPAK_MISSING_SYSCALL_BASE + 447)
#endif
#ifndef __SNR_memfd_secret
# define __SNR_memfd_secret __NR_memfd_secret
#endif

// Last updated: Linux 5.14, syscall numbers < 448
