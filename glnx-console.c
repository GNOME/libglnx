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

#include "config.h"

#include "glnx-console.h"

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>

static char *current_text = NULL;
static gint current_percent = -1;
static gboolean locked;

/* Used by the signal handler.  */
static volatile gboolean signal_stdout_is_tty;

static gboolean
stdout_is_tty (void)
{
  static gsize initialized = 0;
  static gboolean stdout_is_tty_v;

  if (g_once_init_enter (&initialized))
    {
      stdout_is_tty_v = isatty (1);
      g_once_init_leave (&initialized, 1);
    }

  return stdout_is_tty_v;
}

static volatile guint cached_columns = 0;
static volatile guint cached_lines = 0;

static int
fd_columns (int fd)
{
  struct winsize ws = {};

  if (ioctl (fd, TIOCGWINSZ, &ws) < 0)
    return -errno;

  if (ws.ws_col <= 0)
    return -EIO;

  return ws.ws_col;
}

/**
 * glnx_console_columns:
 * 
 * Returns: The number of columns for terminal output
 */
guint
glnx_console_columns (void)
{
  if (G_UNLIKELY (cached_columns == 0))
    {
      int c;

      c = fd_columns (STDOUT_FILENO);

      if (c <= 0)
        c = 80;

      if (c > 256)
        c = 256;

      cached_columns = c;
    }

  return cached_columns;
}

static int
fd_lines (int fd)
{
  struct winsize ws = {};

  if (ioctl (fd, TIOCGWINSZ, &ws) < 0)
    return -errno;

  if (ws.ws_row <= 0)
    return -EIO;

  return ws.ws_row;
}

/**
 * glnx_console_lines:
 * 
 * Returns: The number of lines for terminal output
 */
guint
glnx_console_lines (void)
{
  if (G_UNLIKELY (cached_lines == 0))
    {
      int l;

      l = fd_lines (STDOUT_FILENO);

      if (l <= 0)
        l = 24;

      cached_lines = l;
    }

  return cached_lines;
}

static void
on_sigwinch (int signum)
{
  cached_columns = 0;
  cached_lines = 0;
}

void
glnx_console_lock (GLnxConsoleRef *console)
{
  static gsize sigwinch_initialized = 0;

  g_return_if_fail (!locked);
  g_return_if_fail (!console->locked);

  console->is_tty = stdout_is_tty ();

  locked = console->locked = TRUE;

  current_percent = 0;

  if (console->is_tty)
    {
      if (g_once_init_enter (&sigwinch_initialized))
        {
          signal (SIGWINCH, on_sigwinch);
          g_once_init_leave (&sigwinch_initialized, 1);
        }
      
      { static const char initbuf[] = { '\n', 0x1B, 0x37 };
        (void) fwrite (initbuf, 1, sizeof (initbuf), stdout);
      }
    }
}

static void
printpad (const char *padbuf,
          guint       padbuf_len,
          guint       n)
{
  const guint d = n / padbuf_len;
  const guint r = n % padbuf_len;
  guint i;

  for (i = 0; i < d; i++)
    fwrite (padbuf, 1, padbuf_len, stdout);
  fwrite (padbuf, 1, r, stdout);
}

/**
 * glnx_console_set_color:
 * @fg: Foreground color
 * @fg_style: Foreground style
 * @bg: Background color
 * @bg_style: Background style
 *
 * On a tty, set the output color to @fg and background @bg.  Allow
 * modifiers for the foreground and the background respectively
 * through %fg_style and %bg_style.
 *
 */
void
glnx_console_set_color (GlnxColor fg, GlnxColorStyle fg_style,
                        GlnxColor bg, GlnxColorStyle bg_style)
{
  char buffer[32];
  if (!stdout_is_tty ())
    return;

  if (fg_style)
    {
      int mod = 0;
      int color = fg + 30;

      if (fg_style & GLNX_COLOR_STYLE_BOLD)
          mod = 1;
      if (fg_style & GLNX_COLOR_STYLE_UNDERLINE)
          mod = 4;
      if (fg_style & GLNX_COLOR_STYLE_HIGH_INTENSITY)
          color = fg + 90;

      sprintf (buffer, "\x1B[%i;%im", mod, color);
      fprintf (stdout, buffer);
      fflush (stdout);
    }
  if (bg_style)
    {
      int color;

      if (bg_style & GLNX_COLOR_STYLE_HIGH_INTENSITY)
        color = bg + 100;
      else
        color = bg + 90;

      sprintf (buffer, "\x1B[%im", color);
      fprintf (stdout, buffer);
      fflush (stdout);
    }
}

/**
 * glnx_console_reset_color:
 * @from_signal: Specify if the function is called from a signal.
 *
 * On a tty, reset the foreground and background colors.
 *
 */
void
glnx_console_reset_color (gboolean from_signal)
{
  static char const reset_sequence[] = "\x1b[0m";
  if (! from_signal)
    {
      if (!stdout_is_tty ())
        return;
      fputs (reset_sequence, stdout);
    }
  else
    {
      size_t written = 0;
      if (! signal_stdout_is_tty)
        return;
      while (written < sizeof reset_sequence - 1)
        {
          int ret = write (STDOUT_FILENO, reset_sequence + written,
                           sizeof reset_sequence - 1 - written);
          if (ret < 0)
            return;
          written += ret;
        }
    }
}

static void
color_signal_handler (int sig)
{
  glnx_console_reset_color (TRUE);

  /* Restore the default handler, and report the signal again.  */
  signal (sig, SIG_DFL);
  raise (sig);
}

/**
 * glnx_console_install_signal_handlers:
 * @from_signal: Install the handler to reset the console colors
 * on a signal.
 *
 */
void
glnx_console_install_signal_handlers (void)
{
  int j;
#if HAVE_SIGACTION
  struct sigaction act;
#endif
  int const sig[] =
    {
      SIGTSTP,
      SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT, SIGTERM,
#ifdef SIGPOLL
      SIGPOLL,
#endif
#ifdef SIGPROF
      SIGPROF,
#endif
#ifdef SIGVTALRM
      SIGVTALRM,
#endif
#ifdef SIGXCPU
      SIGXCPU,
#endif
#ifdef SIGXFSZ
      SIGXFSZ,
#endif
    };

  signal_stdout_is_tty = stdout_is_tty ();
  for (j = 0; j < sizeof (sig) / sizeof (*sig); j++)
    {
#if HAVE_SIGACTION
      sigaction (sig[j], NULL, &act);
      if (act.sa_handler != SIG_IGN)
        {
          act.sa_handler = color_signal_handler;
          sigaction (sig[j], &act, NULL);
        }
#else
      signal (sig[j], color_signal_handler);
#endif
    }
}

/**
 * glnx_console_progress_text_percent:
 * @text: Show this text before the progress bar
 * @percentage: An integer in the range of 0 to 100
 *
 * On a tty, print to the console @text followed by an ASCII art
 * progress bar whose percentage is @percentage.  If stdout is not a
 * tty, a more basic line by line change will be printed.
 *
 * You must have called glnx_console_lock() before invoking this
 * function.
 *
 */
void
glnx_console_progress_text_percent (const char *text,
                                    guint percentage)
{
  static const char equals[] = "====================";
  const guint n_equals = sizeof (equals) - 1;
  static const char spaces[] = "                    ";
  const guint n_spaces = sizeof (spaces) - 1;
  const guint ncolumns = glnx_console_columns ();
  const guint bar_min = 10;
  const guint input_textlen = text ? strlen (text) : 0;
  guint textlen;
  guint barlen;

  g_return_if_fail (percentage >= 0 && percentage <= 100);

  if (text && !*text)
    text = NULL;

  if (percentage == current_percent
      && g_strcmp0 (text, current_text) == 0)
    return;

  if (!stdout_is_tty ())
    {
      if (text)
        fprintf (stdout, "%s", text);
      if (percentage != -1)
        {
          if (text)
            fputc (' ', stdout);
          fprintf (stdout, "%u%%", percentage);
        }
      fputc ('\n', stdout);
      fflush (stdout);
      return;
    }

  if (ncolumns < bar_min)
    return; /* TODO: spinner */

  /* Restore cursor */
  { const char beginbuf[2] = { 0x1B, 0x38 };
    (void) fwrite (beginbuf, 1, sizeof (beginbuf), stdout);
  }

  textlen = MIN (input_textlen, ncolumns - bar_min);
  barlen = ncolumns - textlen;

  if (textlen > 0)
    {
      fwrite (text, 1, textlen - 1, stdout);
      fputc (' ', stdout);
    }
  
  { 
    const guint nbraces = 2;
    const guint textpercent_len = 5;
    const guint bar_internal_len = barlen - nbraces - textpercent_len;
    const guint eqlen = bar_internal_len * (percentage / 100.0);
    const guint spacelen = bar_internal_len - eqlen; 

    fputc ('[', stdout);
    printpad (equals, n_equals, eqlen);
    printpad (spaces, n_spaces, spacelen);
    fputc (']', stdout);
    fprintf (stdout, " %3d%%", percentage);
  }

  { const guint spacelen = ncolumns - textlen - barlen;
    printpad (spaces, n_spaces, spacelen);
  }

  fflush (stdout);
}

/**
 * glnx_console_unlock:
 *
 * Print a newline, and reset all cached console progress state.
 *
 * This function does nothing if stdout is not a tty.
 */
void
glnx_console_unlock (GLnxConsoleRef *console)
{
  g_return_if_fail (locked);
  g_return_if_fail (console->locked);

  current_percent = -1;
  g_clear_pointer (&current_text, g_free);

  if (console->is_tty)
    fputc ('\n', stdout);
      
  locked = FALSE;
}
