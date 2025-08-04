/* sysprof-diagnostic.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "sysprof-diagnostic-private.h"

struct _SysprofDiagnostic
{
  GObject parent_instance;
  GRefString *domain;
  char *message;
  guint fatal : 1;
};

enum {
  PROP_0,
  PROP_DOMAIN,
  PROP_MESSAGE,
  PROP_FATAL,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (SysprofDiagnostic, sysprof_diagnostic, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
sysprof_diagnostic_finalize (GObject *object)
{
  SysprofDiagnostic *self = (SysprofDiagnostic *)object;

  g_clear_pointer (&self->domain, g_free);
  g_clear_pointer (&self->message, g_free);

  G_OBJECT_CLASS (sysprof_diagnostic_parent_class)->finalize (object);
}

static void
sysprof_diagnostic_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  SysprofDiagnostic *self = SYSPROF_DIAGNOSTIC (object);

  switch (prop_id)
    {
    case PROP_DOMAIN:
      g_value_set_string (value, self->domain);
      break;

    case PROP_MESSAGE:
      g_value_set_string (value, self->message);
      break;

    case PROP_FATAL:
      g_value_set_boolean (value, self->fatal);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sysprof_diagnostic_class_init (SysprofDiagnosticClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = sysprof_diagnostic_finalize;
  object_class->get_property = sysprof_diagnostic_get_property;

  properties [PROP_DOMAIN] =
    g_param_spec_string ("domain", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MESSAGE] =
    g_param_spec_string ("message", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FATAL] =
    g_param_spec_boolean ("fatal", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
sysprof_diagnostic_init (SysprofDiagnostic *self)
{
}

const char *
sysprof_diagnostic_get_domain (SysprofDiagnostic *self)
{
  g_return_val_if_fail (SYSPROF_IS_DIAGNOSTIC (self), NULL);

  return self->domain;
}

const char *
sysprof_diagnostic_get_message (SysprofDiagnostic *self)
{
  g_return_val_if_fail (SYSPROF_IS_DIAGNOSTIC (self), NULL);

  return self->message;
}

gboolean
sysprof_diagnostic_get_fatal (SysprofDiagnostic *self)
{
  g_return_val_if_fail (SYSPROF_IS_DIAGNOSTIC (self), FALSE);

  return self->fatal;
}

SysprofDiagnostic *
_sysprof_diagnostic_new (char     *domain,
                         char     *message,
                         gboolean  fatal)
{
  SysprofDiagnostic *self;

  self = g_object_new (SYSPROF_TYPE_DIAGNOSTIC, NULL);
  self->message = message;
  self->domain = domain;
  self->fatal = !!fatal;

  return self;
}
