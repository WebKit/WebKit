#include "mapped-ring-buffer.h"

#include <glib.h>
#include <stdint.h>

static gsize real_count;

static bool
drain_nth_cb (const void *data,
              size_t     *len,
              void       *user_data)
{
  const gint64 *v64 = data;
  g_assert_cmpint (*len, >=, 8);
  g_assert_cmpint (*v64, ==, GPOINTER_TO_SIZE (user_data));
  *len = sizeof *v64;
  return G_SOURCE_CONTINUE;
}

static bool
drain_count_cb (const void *data,
                size_t     *len,
                void       *user_data)
{
  const gint64 *v64 = data;
  g_assert_cmpint (*len, >=, 8);
  ++real_count;
  g_assert_cmpint (real_count, ==, *v64);
  *len = sizeof *v64;
  return G_SOURCE_CONTINUE;
}

static void
test_basic_movements (void)
{
  MappedRingBuffer *reader;
  MappedRingBuffer *writer;
  gint64 *ptr;
  guint i;
  int fd;

  reader = mapped_ring_buffer_new_reader (4096*16);
  g_assert_nonnull (reader);

  fd = mapped_ring_buffer_get_fd (reader);
  g_assert_cmpint (fd, >, -1);

  writer = mapped_ring_buffer_new_writer (fd);
  g_assert_nonnull (writer);

  while ((ptr = mapped_ring_buffer_allocate (writer, sizeof *ptr)))
    {
      static gint64 count;
      g_assert ((GPOINTER_TO_SIZE(ptr) & 0x7) == 0);
      *ptr = ++count;
      mapped_ring_buffer_advance (writer, sizeof *ptr);
    }
  mapped_ring_buffer_drain (reader, drain_count_cb, NULL);

  ptr = mapped_ring_buffer_allocate (writer, sizeof *ptr);
  g_assert_nonnull (ptr);
  *ptr = 1000000;
  mapped_ring_buffer_advance (writer, sizeof *ptr);
  mapped_ring_buffer_drain (reader, drain_nth_cb, GSIZE_TO_POINTER (1000000));

  real_count = 0;
  for (i = 0; i < g_random_int_range (1, 4000); i++)
    {
      static gint64 count;
      g_assert ((GPOINTER_TO_SIZE(ptr) & 0x7) == 0);
      *ptr = ++count;
      mapped_ring_buffer_advance (writer, sizeof *ptr);
    }
  mapped_ring_buffer_drain (reader, drain_count_cb, NULL);

  real_count = 0;
  while ((ptr = mapped_ring_buffer_allocate (writer, sizeof *ptr)))
    {
      static gint64 count;
      g_assert ((GPOINTER_TO_SIZE(ptr) & 0x7) == 0);
      *ptr = ++count;
      mapped_ring_buffer_advance (writer, sizeof *ptr);
    }
  mapped_ring_buffer_drain (reader, drain_count_cb, NULL);

  mapped_ring_buffer_unref (writer);
  mapped_ring_buffer_unref (reader);
}

typedef struct
{
  gint64 abc;
  gint64 done;
} ThreadedMessage;

static bool
handle_msg (const void *data,
            size_t     *length,
            void       *user_data)
{
  const ThreadedMessage *msg = data;
  gboolean *done = user_data;
  *done = msg->done;
  *length = sizeof *msg;
  return G_SOURCE_CONTINUE;
}

static void *
threaded_reader (gpointer data)
{
  MappedRingBuffer *reader = data;
  gboolean done = FALSE;

  while (!done)
    mapped_ring_buffer_drain (reader, handle_msg, &done);

  return NULL;
}

static void *
threaded_writer (gpointer data)
{
  MappedRingBuffer *writer = data;
  ThreadedMessage *msg;

  for (guint i = 0; i < 100000; i++)
    {
      while (!(msg = mapped_ring_buffer_allocate (writer, sizeof *msg)))
        g_usleep (G_USEC_PER_SEC / 10000); /* .1msec */

      msg->done = FALSE;
      mapped_ring_buffer_advance (writer, sizeof *msg);
    }

  while (!(msg = mapped_ring_buffer_allocate (writer, sizeof *msg)))
    g_usleep (G_USEC_PER_SEC / 10000); /* .1msec */

  msg->done = TRUE;
  mapped_ring_buffer_advance (writer, sizeof *msg);

  return NULL;
}

static void
test_threaded_movements (void)
{
  GThread *thread1, *thread2;
  MappedRingBuffer *reader;
  MappedRingBuffer *writer;
  int fd;

  reader = mapped_ring_buffer_new_reader (4096*16);
  g_assert_nonnull (reader);

  fd = mapped_ring_buffer_get_fd (reader);
  g_assert_cmpint (fd, >, -1);

  writer = mapped_ring_buffer_new_writer (fd);
  g_assert_nonnull (writer);

  thread1 = g_thread_new ("thread1-reader", threaded_reader, reader);
  thread2 = g_thread_new ("thread2-writer", threaded_writer, writer);

  g_thread_join (thread1);
  g_thread_join (thread2);

  mapped_ring_buffer_unref (writer);
  mapped_ring_buffer_unref (reader);
}

static void
test_readwrite (void)
{
  MappedRingBuffer *ring;
  gint64 *ptr;

  ring = mapped_ring_buffer_new_readwrite (4096*16);
  g_assert_nonnull (ring);

  real_count = 0;
  while ((ptr = mapped_ring_buffer_allocate (ring, sizeof *ptr)))
    {
      static gint64 count;
      g_assert ((GPOINTER_TO_SIZE(ptr) & 0x7) == 0);
      *ptr = ++count;
      mapped_ring_buffer_advance (ring, sizeof *ptr);
    }
  mapped_ring_buffer_drain (ring, drain_count_cb, NULL);

  mapped_ring_buffer_unref (ring);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/MappedRingBuffer/basic_movements", test_basic_movements);
  g_test_add_func ("/MappedRingBuffer/readwrite", test_readwrite);
  g_test_add_func ("/MappedRingBuffer/threaded_movements", test_threaded_movements);
  return g_test_run ();
}
