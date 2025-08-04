/* sysprof-spawnable.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <unistd.h>

#include <glib/gstdio.h>

#include "sysprof-spawnable.h"

typedef struct
{
  gint dest_fd;
  gint fd;
} FDMapping;

struct _SysprofSpawnable
{
  GObject           parent_instance;
  GArray           *fds;
  GPtrArray        *argv;
  char            **environ;
  char             *cwd;
  gint              next_fd;
  GSubprocessFlags  flags;
};

G_DEFINE_TYPE (SysprofSpawnable, sysprof_spawnable, G_TYPE_OBJECT)

/**
 * sysprof_spawnable_new:
 *
 * Create a new #SysprofSpawnable.
 *
 * Returns: (transfer full): a newly created #SysprofSpawnable
 */
SysprofSpawnable *
sysprof_spawnable_new (void)
{
  return g_object_new (SYSPROF_TYPE_SPAWNABLE, NULL);
}

/**
 * sysprof_spawnable_get_flags:
 * @self: a #SysprofSpawnable
 *
 * Gets the subprocess flags for spawning.
 *
 * Returns: the #GSubprocessFlags bitwise-or'd
 */
GSubprocessFlags
sysprof_spawnable_get_flags (SysprofSpawnable *self)
{
  g_return_val_if_fail (SYSPROF_IS_SPAWNABLE (self), 0);

  return self->flags;
}

/**
 * sysprof_spawnable_set_flags:
 * @self: a #SysprofSpawnable
 *
 * Set the flags to use when spawning the process.
 */
void
sysprof_spawnable_set_flags (SysprofSpawnable *self,
                             GSubprocessFlags  flags)
{
  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));

  self->flags = flags;
}

static void
fd_mapping_clear (gpointer data)
{
  FDMapping *map = data;

  if (map->fd != -1)
    close (map->fd);
}

static void
sysprof_spawnable_finalize (GObject *object)
{
  SysprofSpawnable *self = (SysprofSpawnable *)object;

  g_clear_pointer (&self->fds, g_array_unref);
  g_clear_pointer (&self->argv, g_ptr_array_unref);
  g_clear_pointer (&self->environ, g_strfreev);

  G_OBJECT_CLASS (sysprof_spawnable_parent_class)->finalize (object);
}

static void
sysprof_spawnable_class_init (SysprofSpawnableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = sysprof_spawnable_finalize;
}

static void
sysprof_spawnable_init (SysprofSpawnable *self)
{
  self->next_fd = 3;

  self->environ = g_get_environ ();

  self->argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (self->argv, NULL);

  self->fds = g_array_new (FALSE, FALSE, sizeof (FDMapping));
  g_array_set_clear_func (self->fds, fd_mapping_clear);
}

void
sysprof_spawnable_prepend_argv (SysprofSpawnable *self,
                                const gchar      *argv)
{
  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));

  if (argv != NULL)
    g_ptr_array_insert (self->argv, 0, g_strdup (argv));
}

void
sysprof_spawnable_append_argv (SysprofSpawnable *self,
                               const gchar      *argv)
{
  gint pos;

  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));

  if (argv == NULL)
    return;

  pos = self->argv->len - 1;
  g_ptr_array_add (self->argv, NULL);
  g_ptr_array_index (self->argv, pos) = g_strdup (argv);
}

void
sysprof_spawnable_append_args (SysprofSpawnable    *self,
                               const gchar * const *args)
{
  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));

  if (args == NULL)
    return;

  for (guint i = 0; args[i]; i++)
    sysprof_spawnable_append_argv (self, args[i]);
}

const gchar * const *
sysprof_spawnable_get_argv (SysprofSpawnable *self)
{
  g_return_val_if_fail (SYSPROF_IS_SPAWNABLE (self), NULL);

  return (const gchar * const *)(gpointer)self->argv->pdata;
}

const gchar * const *
sysprof_spawnable_get_environ (SysprofSpawnable *self)
{
  g_return_val_if_fail (SYSPROF_IS_SPAWNABLE (self), NULL);

  return (const gchar * const *)self->environ;
}

void
sysprof_spawnable_setenv (SysprofSpawnable *self,
                          const gchar      *key,
                          const gchar      *value)
{
  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));
  g_return_if_fail (key != NULL);

  self->environ = g_environ_setenv (self->environ, key, value, TRUE);
}

void
sysprof_spawnable_set_environ (SysprofSpawnable    *self,
                               const gchar * const *environ_)
{
  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));

  if (environ_ != (const gchar * const *)self->environ)
    {
      g_strfreev (self->environ);
      self->environ = g_strdupv ((gchar **)environ_);
    }
}

const gchar *
sysprof_spawnable_getenv (SysprofSpawnable *self,
                          const gchar      *key)
{
  g_return_val_if_fail (SYSPROF_IS_SPAWNABLE (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_environ_getenv (self->environ, key);
}

gint
sysprof_spawnable_take_fd (SysprofSpawnable *self,
                           gint              fd,
                           gint              dest_fd)
{
  FDMapping map;

  g_return_val_if_fail (SYSPROF_IS_SPAWNABLE (self), -1);

  if (dest_fd < 0)
    dest_fd = self->next_fd++;

  map.dest_fd = dest_fd;
  map.fd = fd;

  if (dest_fd >= self->next_fd)
    self->next_fd = dest_fd + 1;

  g_array_append_val (self->fds, map);

  return dest_fd;
}

void
sysprof_spawnable_foreach_fd (SysprofSpawnable          *self,
                              SysprofSpawnableFDForeach  foreach,
                              gpointer                   user_data)
{
  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));
  g_return_if_fail (foreach != NULL);

  for (guint i = 0; i < self->fds->len; i++)
    {
      const FDMapping *map = &g_array_index (self->fds, FDMapping, i);

      foreach (map->dest_fd, map->fd, user_data);
    }
}

/**
 * sysprof_spawnable_set_starting_fd:
 *
 * Sets the next FD number to use when mapping a child FD. This helps
 * in situations where the embedder knows that some lower-numbered FDs
 * will be taken and therefore unknown to the spawnable.
 *
 * The default for this is 2.
 */
void
sysprof_spawnable_set_starting_fd (SysprofSpawnable *self,
                                   gint              starting_fd)
{
  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));

  if (starting_fd < 0)
    starting_fd = 2;

  self->next_fd = starting_fd;
}

/**
 * sysprof_spawnable_spawn:
 *
 * Creates a new subprocess using the configured options.
 *
 * Returns: (transfer full): a #GSubprocess or %NULL on failure and
 *   @error is set.
 */
GSubprocess *
sysprof_spawnable_spawn (SysprofSpawnable  *self,
                         GError           **error)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  const gchar * const *argv;

  g_return_val_if_fail (SYSPROF_IS_SPAWNABLE (self), NULL);

  launcher = g_subprocess_launcher_new (self->flags);

  g_subprocess_launcher_set_environ (launcher, self->environ);

  if (self->cwd != NULL)
    g_subprocess_launcher_set_cwd (launcher, self->cwd);
  else
    g_subprocess_launcher_set_cwd (launcher, g_get_home_dir ());

  for (guint i = 0; i < self->fds->len; i++)
    {
      FDMapping *map = &g_array_index (self->fds, FDMapping, i);

      g_subprocess_launcher_take_fd (launcher, map->fd, map->dest_fd);
      map->fd = -1;
    }

  argv = sysprof_spawnable_get_argv (self);

  return g_subprocess_launcher_spawnv (launcher, argv, error);
}

const char *
sysprof_spawnable_get_cwd (SysprofSpawnable *self)
{
  g_return_val_if_fail (SYSPROF_IS_SPAWNABLE (self), NULL);

  return self->cwd;
}

void
sysprof_spawnable_set_cwd (SysprofSpawnable *self,
                           const gchar      *cwd)
{
  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));

  g_set_str (&self->cwd, cwd);
}

/**
 * sysprof_spawnable_add_trace_fd:
 * @self: a #SysprofSpawnable
 * @envvar: (nullable): the environment variable
 *
 * Adds an environment variable to the spawnable that will contain a
 * "tracing file-descriptor". The spawned process can use
 * `sysprof_capture_writer_new_from_env()` if @envvar is %NULL
 * or with `getenv()` and `sysprof_capture_writer_new_from_fd()`.
 *
 * If @envvar is %NULL, "SYSPROF_TRACE_FD" will be used.
 *
 * The caller is responsible for closin the resulting FD.
 *
 * Returns: A file-descriptor which can be used to read the trace or
 *   -1 upon failure and `errno` is set. Caller must `close()` the
 *   FD if >= 0.
 */
int
sysprof_spawnable_add_trace_fd (SysprofSpawnable *self,
                                const char       *envvar)
{
  g_autofd int fd = -1;
  g_autofd int dest = -1;
  g_autofree char *name = NULL;
  g_autofree char *fdstr = NULL;
  int id_in_child;

  g_return_val_if_fail (SYSPROF_IS_SPAWNABLE (self), -1);

  if (envvar == NULL)
    envvar = "SYSPROF_TRACE_FD";

  name = g_strdup_printf ("[sysprof-tracefd:%s]", envvar);

  if (-1 == (fd = sysprof_memfd_create (name)))
    return -1;

  if (-1 == (dest = dup (fd)))
    return -1;

  id_in_child = sysprof_spawnable_take_fd (self, g_steal_fd (&dest), -1);

  fdstr = g_strdup_printf ("%d", id_in_child);
  sysprof_spawnable_setenv (self, envvar, fdstr);

  return g_steal_fd (&fd);
}

void
sysprof_spawnable_add_ld_preload (SysprofSpawnable *self,
                                  const char       *library_path)
{
  g_autofree char *amended = NULL;
  const char *val;

  g_return_if_fail (SYSPROF_IS_SPAWNABLE (self));
  g_return_if_fail (library_path != NULL);

  if ((val = sysprof_spawnable_getenv (self, "LD_PRELOAD")))
    library_path = amended = g_strdup_printf ("%s:%s", val, library_path);

  sysprof_spawnable_setenv (self, "LD_PRELOAD", library_path);
}
