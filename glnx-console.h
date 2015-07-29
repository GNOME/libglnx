/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013,2014,2015 Colin Walters <walters@verbum.org>
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#pragma once

#include <glnx-backport-autocleanups.h>

G_BEGIN_DECLS

struct GLnxConsoleRef {
  gboolean locked;
  gboolean is_tty;
};

typedef enum {
  GLNX_COLOR_STYLE_REGULAR          = (1 << 0),
  GLNX_COLOR_STYLE_BOLD             = (1 << 1),
  GLNX_COLOR_STYLE_UNDERLINE        = (1 << 2),
  GLNX_COLOR_STYLE_HIGH_INTENSITY   = (1 << 3),
} GlnxColorStyle;

typedef enum {
  GLNX_COLOR_BLACK = 0,
  GLNX_COLOR_RED = 1,
  GLNX_COLOR_GREEN = 2,
  GLNX_COLOR_YELLOW = 3,
  GLNX_COLOR_BLUE = 4,
  GLNX_COLOR_PURPLE = 5,
  GLNX_COLOR_CYAN = 6,
  GLNX_COLOR_WHITE = 7,
} GlnxColor;

typedef struct GLnxConsoleRef GLnxConsoleRef;

void	 glnx_console_lock (GLnxConsoleRef *ref);

void	 glnx_console_progress_text_percent (const char     *text,
                                             guint           percentage);

void	 glnx_console_unlock (GLnxConsoleRef *ref);

guint    glnx_console_lines (void);

guint    glnx_console_columns (void);

void     glnx_console_set_color (GlnxColor fg, GlnxColorStyle fg_style,
                         GlnxColor bg, GlnxColorStyle bg_style);
void     glnx_console_reset_color (gboolean from_signal);
void     glnx_console_install_signal_handlers (void);

static inline void
glnx_console_ref_cleanup (GLnxConsoleRef *p)
{
  glnx_console_unlock (p);
}
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(GLnxConsoleRef, glnx_console_ref_cleanup)

G_END_DECLS
