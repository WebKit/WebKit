/*
 * test-channel.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libdex.h>

#define ASSERT_STATUS(f,status) g_assert_cmpint(status, ==, dex_future_get_status(DEX_FUTURE(f)))

#define ASSERT_CMP(future, kind, get, op, v) \
  G_STMT_START { \
    GError *error = NULL; \
    const GValue *value = dex_future_get_value (DEX_FUTURE (future), &error); \
    g_assert_no_error (error); \
    g_assert_nonnull (value); \
    G_PASTE (g_assert_cmp, kind) (G_PASTE (g_value_get_, get) (value), op, v); \
  } G_STMT_END
#define ASSERT_CMPINT(future, op, value) ASSERT_CMP(future, int, int, op, value)
#define ASSERT_CMPUINT(future, op, value) ASSERT_CMP(future, int, uint, op, value)

static void
test_channel_basic (void)
{
  DexChannel *channel;
  DexFuture *value1 = NULL;
  DexFuture *value2 = NULL;
  DexFuture *value3 = NULL;
  DexFuture *send1 = NULL;
  DexFuture *send2 = NULL;
  DexFuture *send3 = NULL;
  DexFuture *recv1 = NULL;
  DexFuture *recv2 = NULL;
  DexFuture *recv3 = NULL;

  channel = dex_channel_new (2);
  g_assert_true (dex_channel_can_send (channel));
  g_assert_true (dex_channel_can_receive (channel));

  value1 = dex_future_new_for_int (1);
  value2 = dex_future_new_for_int (2);
  value3 = dex_future_new_for_int (3);
  ASSERT_CMPINT (value1, ==, 1);
  ASSERT_CMPINT (value2, ==, 2);
  ASSERT_CMPINT (value3, ==, 3);

  send1 = dex_channel_send (channel, dex_ref (value1));
  g_assert_true ((gpointer)send1 != (gpointer)value1);
  g_assert_true (dex_channel_can_send (channel));
  g_assert_true (dex_channel_can_receive (channel));
  ASSERT_STATUS (send1, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_CMPUINT (send1, ==, 1);

  send2 = dex_channel_send (channel, dex_ref (value2));
  g_assert_true (dex_channel_can_send (channel));
  g_assert_true (dex_channel_can_receive (channel));
  ASSERT_STATUS (send2, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_CMPUINT (send2, ==, 2);

  send3 = dex_channel_send (channel, dex_ref (value3));
  g_assert_true (dex_channel_can_send (channel));
  g_assert_true (dex_channel_can_receive (channel));
  ASSERT_STATUS (send3, DEX_FUTURE_STATUS_PENDING);

  dex_channel_close_send (channel);
  g_assert_false (dex_channel_can_send (channel));
  g_assert_true (dex_channel_can_receive (channel));
  ASSERT_STATUS (send3, DEX_FUTURE_STATUS_PENDING);

  recv1 = dex_channel_receive (channel);
  ASSERT_STATUS (send3, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_STATUS (recv1, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_CMPUINT (send3, ==, 2);
  ASSERT_CMPINT (recv1, ==, 1);

  recv2 = dex_channel_receive (channel);
  ASSERT_STATUS (recv2, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_CMPINT (recv2, ==, 2);

  dex_channel_close_receive (channel);
  g_assert_false (dex_channel_can_send (channel));
  g_assert_false (dex_channel_can_receive (channel));

  recv3 = dex_channel_receive (channel);
  ASSERT_STATUS (recv3, DEX_FUTURE_STATUS_REJECTED);

  dex_clear (&value1);
  dex_clear (&value2);
  dex_clear (&value3);
  dex_clear (&send1);
  dex_clear (&send2);
  dex_clear (&send3);
  dex_clear (&recv1);
  dex_clear (&recv2);
  dex_clear (&recv3);
  dex_clear (&channel);
}

static void
test_channel_recv_first (void)
{
  DexChannel *channel = dex_channel_new (2);
  DexFuture *recv1 = dex_channel_receive (channel);
  DexFuture *recv2 = dex_channel_receive (channel);
  DexFuture *recv3 = dex_channel_receive (channel);
  DexFuture *recv4;
  DexFuture *value1 = dex_future_new_for_int (123);
  DexFuture *send1;

  ASSERT_STATUS (recv1, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (recv2, DEX_FUTURE_STATUS_PENDING);

  send1 = dex_channel_send (channel, dex_ref (value1));
  ASSERT_STATUS (send1, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_STATUS (recv1, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_STATUS (recv2, DEX_FUTURE_STATUS_PENDING);

  dex_channel_close_send (channel);
  ASSERT_STATUS (recv2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (recv3, DEX_FUTURE_STATUS_REJECTED);

  recv4 = dex_channel_receive (channel);
  ASSERT_STATUS (recv4, DEX_FUTURE_STATUS_REJECTED);

  dex_clear (&channel);
  dex_clear (&recv1);
  dex_clear (&recv2);
  dex_clear (&recv3);
  dex_clear (&recv4);
  dex_clear (&value1);
  dex_clear (&send1);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Dex/TestSuite/Channel/basic", test_channel_basic);
  g_test_add_func ("/Dex/TestSuite/Channel/recv_first", test_channel_recv_first);
  return g_test_run ();
}
