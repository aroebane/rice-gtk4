/* rice-gtk4 core: theme record -> GTK 4 CSS text. No GTK in here.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RICEGTK4_CSS_H
#define RICEGTK4_CSS_H

#include "theme.h"

/* The whole stylesheet: @define-color lines, then the template with the shape numbers filled
 * in. Free with g_free. NULL and *err set if the template names a token nobody defines. */
char *r4_css_render(const r4_theme *t, GError **err);

/* index.theme for the theme dir (Name=rice). Free with g_free. */
char *r4_index_theme(const r4_theme *t);

#endif
