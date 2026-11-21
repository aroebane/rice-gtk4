/* rice-gtk4 core: config + palette file -> one theme record, derived with rice-colors.
 * No GTK in here, so the generator tools stay small.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RICEGTK4_THEME_H
#define RICEGTK4_THEME_H

#include <glib.h>
#include <rice-colors/rice-colors.h>

#include "design.h"

typedef ricecol_rgba_t r4_color;

typedef enum {
    R4_CTL_SELECTION,
    R4_CTL_SELECTION_TEXT,
    R4_CTL_TRACK,
    R4_CTL_TRACK_HOVER,
    R4_CTL_THUMB,
    R4_CTL_THUMB_HOVER,
    R4_CTL_THUMB_ACTIVE,
    R4_CTL_VISITED,
    R4_CTL_TAB_TEXT,
    R4_CTL_DISABLED_STRONG,
    R4_CTL_BORDER_SUBTLE,
    R4_CTL_BORDER_RULE,
    R4_CTL_BORDER_RESTING_LINK,
    /* The shell's own state colours; each replaces the engine's derivation of the same name. */
    R4_CTL_HOVER_SURFACE,
    R4_CTL_PRESSED_SURFACE,
    R4_CTL_CONTAINER_HOVER,
    R4_CTL_CONTAINER_PRESSED,
    R4_CTL_PRIMARY_HOVER,
    R4_CTL_PRIMARY_PRESSED,
    R4_CTL_DISABLED_TEXT,
    R4_CTL_COUNT
} r4_ctl;

/* What the producer's behaviour profile paints, per emphasis and state ([Control.outline],
 * [Control.flat], [Control.solid] of the palette file). The file names a role; the engine gives the
 * colour, so its own overrides and the contrast guard still apply. */
typedef enum { R4_PE_OUTLINE, R4_PE_FLAT, R4_PE_SOLID, R4_PE_COUNT } r4_paint_emph;
typedef enum {
    R4_PS_REST_BORDER,
    R4_PS_HOVER_BORDER,
    R4_PS_PRESSED_FILL,
    R4_PS_PRESSED_BORDER,
    R4_PS_SELECT_FILL,
    R4_PS_SELECT_TEXT,
    R4_PS_SELECT_BORDER,
    R4_PS_DISABLED_FILL,
    R4_PS_DISABLED_BORDER,
    R4_PS_COUNT
} r4_paint_slot;
typedef enum {
    R4_ROLE_UNSET, /* the slot was not given: the engine's own look */
    R4_ROLE_NONE,
    R4_ROLE_SURFACE,
    R4_ROLE_HOVER_SURFACE,
    R4_ROLE_FOCUS_SURFACE,
    R4_ROLE_PRESSED_SURFACE,
    R4_ROLE_CONTAINER,
    R4_ROLE_CONTAINER_HOVER,
    R4_ROLE_CONTAINER_PRESSED,
    R4_ROLE_CURRENT_CONTAINER,
    R4_ROLE_DISABLED_SURFACE,
    R4_ROLE_PRIMARY,
    R4_ROLE_PRIMARY_HOVER,
    R4_ROLE_PRIMARY_FOCUS,
    R4_ROLE_PRIMARY_PRESSED,
    R4_ROLE_TEXT,
    R4_ROLE_MUTED,
    R4_ROLE_CONTAINER_TEXT,
    R4_ROLE_CURRENT_TEXT,
    R4_ROLE_DISABLED_TEXT,
    R4_ROLE_PRIMARY_TEXT,
    R4_ROLE_OUTLINE,
    R4_ROLE_FOCUS,
    R4_ROLE_HOVER,
    R4_ROLE_CURRENT,
    R4_ROLE_DISABLED_BORDER,
    R4_ROLE_COUNT
} r4_role;

/* Config-driven behaviour of the states (sections [State] [Contrast] [Focus] [Selection]
 * [Motion] of gtk4.conf). Every field has a range check. */
typedef enum { R4_FOCUS_INSET, R4_FOCUS_OUTER, R4_FOCUS_BORDER } r4_focus_mode;

typedef struct {
    double hover_layer;        /* text washed over a fill on hover, 0..0.5 */
    double pressed_layer;      /* same, pressed, 0..0.5 */
    double disabled_content;   /* text weight of a disabled control over its surface, 0..1 */
    double disabled_container; /* fill weight of a disabled control over the window, 0..1 */
    double text_contrast;      /* floor for text roles against their surfaces, 1..21 */
    double boundary_contrast;  /* floor for borders, focus and accent, 1..21 */
    gboolean enforce;          /* TRUE: raise a failing role; FALSE: only report it */
    r4_focus_mode focus_mode;
    gboolean selection_solid;    /* TRUE: selection is the primary fill, FALSE: tonal container */
    gboolean selection_boundary; /* a selected control draws a boundary in the primary colour */
    gboolean motion;             /* colour-only state transitions */
    int motion_ms;               /* 0..1000 */
    double status_tint;          /* status colour weight over the base in an infobar fill, 0..1 */
    /* FALSE: a hovered control looks like a resting one (keyboard-first) */
    gboolean pointer_hover;
    /* the ring shows on a pointer-focused control too, not only by keyboard */
    gboolean focus_always;
} r4_knobs;

/* [Emphasis]: how a family of controls rests. DEFAULT leaves the template's choice. */
typedef enum { R4_EMPH_DEFAULT, R4_EMPH_OUTLINE, R4_EMPH_FLAT, R4_EMPH_SOLID } r4_emphasis;
typedef enum {
    R4_EF_BUTTON,
    R4_EF_INPUT,
    R4_EF_TAB,
    R4_EF_ROW,
    R4_EF_MENUITEM,
    R4_EF_COUNT
} r4_emph_family;

/* [Element.<manifest id>]: per-element override of the resting look. Colours are stored as CSS
 * text already validated (`@role` or `#rrggbb[aa]` or `transparent`), numbers as -1 when unset. */
typedef struct {
    char *id; /* a manifest id, checked against element-hooks.yaml */
    char *fill, *text, *border;
    int radius, border_width, pad_x, pad_y, min_height;
    r4_emphasis emphasis;
} r4_element_over;

typedef struct {
    /* The fills and text the toolkit's five states are built from. */
    r4_color window, text, base, button, accent, border, focus, muted;
    r4_color container, container_text, primary, primary_text;
    r4_color positive, warning, negative;
    /* Optional producer roles: surfaceVariant, scrim, and the [Control] extension keys. */
    r4_color surface_variant, scrim;
    r4_color ctl[R4_CTL_COUNT];
    guint32 ctl_set; /* bit i: ctl[i] was given */
    r4_role paint[R4_PE_COUNT][R4_PS_COUNT];
    r4_shape shape;
    r4_knobs knobs;
    GPtrArray *corrections; /* roles the contrast guard raised or found failing */
    r4_emphasis emphasis[R4_EF_COUNT];
    GPtrArray *elements; /* r4_element_over*, in file order */
    r4_design design;
    r4_diag diag; /* clamps and downgrades; problems/unknown above stay the theme's */
    gboolean dark;
    char *font_general; /* NULL: leave GTK's choice */
    char *icon_theme;   /* NULL: leave GTK's choice */
    /* Each "[Section] Key" no reader used, for a typo report. */
    GPtrArray *unknown;
    GPtrArray *problems;
    char *config_path;
} r4_theme;

/* $RICE_GTK4_CONFIG, else $XDG_CONFIG_HOME/rice/gtk4.conf, else ~/.config/rice/gtk4.conf. */
char *r4_config_path(void);
/* Loads path (NULL: r4_config_path()). Never fails: missing files give the built-in look. */
r4_theme *r4_theme_load(const char *path);
void r4_theme_free(r4_theme *t);

/* State layers: text washed over the fill (knobs.hover_layer, knobs.pressed_layer). */
r4_color r4_hover(const r4_theme *t, r4_color fill);
r4_color r4_pressed(const r4_theme *t, r4_color fill);
r4_color r4_disabled_fill(const r4_theme *t, r4_color fill);
r4_color r4_disabled_text(const r4_theme *t, r4_color text);
r4_color r4_mix(r4_color a, r4_color b, double bias_to_b);
double r4_contrast(r4_color a, r4_color b);
r4_color r4_readable_on(r4_color fill, r4_color dark, r4_color light);

void r4_element_over_free(gpointer p);

/* "#rrggbb" into buf (at least 8 bytes). */
void r4_hex(r4_color c, char buf[8]);

#endif
