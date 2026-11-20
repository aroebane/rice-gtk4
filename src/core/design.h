/* rice-gtk4 core: design profiles for GTK 4. The generic half (find, inherit, validate, resolve
 * what a family asks for) is rice-design. This file is the GTK 4 half: what CSS can draw, the
 * downgrade from "asked" to "drawn", and the CSS value. It knows nothing about GTK itself.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RICEGTK4_DESIGN_H
#define RICEGTK4_DESIGN_H

#include <glib.h>
#include <rice-design/rice-design.h>
#include <rice-ini/rice-ini.h>

/* The numbers the stylesheet is built from (the legacy [Shape] block, now also set by designs). */
typedef struct {
    double radius;       /* legacy [Shape] Radius */
    gboolean radius_set; /* only a config that names Radius overrides the design's radii */
    int border_width, focus_width, focus_offset, spacing, padding;
    int control_height, scrollbar_width, indicator_size;
} r4_shape;

typedef ricedes_family_t r4_family;
typedef ricedes_treatment_t r4_treatment;
#define R4_FAM_WINDOW RICEDES_WINDOW
#define R4_FAM_FRAME RICEDES_FRAME
#define R4_FAM_SURFACE RICEDES_SURFACE
#define R4_FAM_DIALOG RICEDES_DIALOG
#define R4_FAM_HEADERBAR RICEDES_HEADERBAR
#define R4_FAM_BUTTON RICEDES_BUTTON
#define R4_FAM_INPUT RICEDES_INPUT
#define R4_FAM_MENU RICEDES_MENU
#define R4_FAM_POPOVER RICEDES_POPOVER
#define R4_FAM_TOOLTIP RICEDES_TOOLTIP
#define R4_FAM_TAB RICEDES_TAB
#define R4_FAM_ROW RICEDES_ROW
#define R4_FAM_BADGE RICEDES_BADGE
#define R4_FAM_INDICATOR RICEDES_INDICATOR
#define R4_FAM_CHECK RICEDES_CHECK
#define R4_FAM_RADIO RICEDES_RADIO
#define R4_FAM_SWITCH RICEDES_SWITCH
#define R4_FAM_SCROLLBAR RICEDES_SCROLLBAR
#define R4_FAM_SLIDER RICEDES_SLIDER
#define R4_FAM_PROGRESS RICEDES_PROGRESS
#define R4_FAM_FOCUS RICEDES_FOCUS
#define R4_FAM_COUNT RICEDES_FAMILY_COUNT
#define R4_TR_SHARP RICEDES_TR_SHARP
#define R4_TR_ROUNDED RICEDES_TR_ROUNDED
#define R4_TR_CHAMFERED RICEDES_TR_CHAMFERED
#define R4_TR_CUT RICEDES_TR_CUT
#define R4_TR_CAPSULE RICEDES_TR_CAPSULE
#define r4_family_name ricedes_family_name
#define r4_treatment_name ricedes_treatment_name

/* What the toolkit can draw. A length it cannot draw is rounded to whole px and reported. */
typedef struct {
    gboolean fractional_radius; /* GTK 4 CSS parses fractional border-radius */
    gboolean fractional_border; /* GTK rounds border widths */
} r4_caps;

typedef struct {
    r4_caps caps;
    ricedes_t *des; /* the generic design state */
} r4_design;

/* What the loaders say about the input, shared with the theme. Each entry is one line of text.
 * The arrays are borrowed: whoever builds the struct owns them. */
typedef struct {
    GPtrArray *problems;   /* the input is wrong or unusable */
    GPtrArray *unknown;    /* "[Section] Key (file)" nobody reads */
    GPtrArray *clamps;     /* a value moved into its valid range */
    GPtrArray *downgrades; /* a treatment GTK 4 CSS cannot draw, and what replaced it */
} r4_diag;

/* Corner geometry of one family at one size. Corner order is CSS order (TL TR BR BL). */
typedef struct {
    r4_treatment asked[4], drawn[4];
    double radius[4];     /* px of the drawn border-radius; full = R4_FULL_RADIUS */
    double asked_size[4]; /* radius, chamfer or cut size that was asked for */
} r4_geometry;

/* A radius that rounds a whole edge: GTK clamps border-radius to the box,
 * so a very large value is a full pill for any size. */
#define R4_FULL_RADIUS 9999.0

void r4_design_init(r4_design *d);
void r4_design_clear(r4_design *d);

/* Finds and applies the design the config names ($RICE_GTK4_DESIGN_PATH, $RICE_GTK4_DESIGN, or
 * [Design] Path/Name in cfg), with its Inherits chain, and copies its metrics into s. A broken
 * chain applies nothing. */
void r4_design_load(
    r4_design *d, r4_shape *s, const riceini_t *cfg, const char *cfg_path, const r4_diag *dg);

/* Applies the config's [Shape.<family>] blocks on top of the design. */
void r4_design_apply_config(r4_design *d,
                            const riceini_t *cfg,
                            const char *file,
                            const r4_diag *dg);

/* The static box (w x h px, 0: not known) a family's corner radius is checked against. */
void r4_family_box(const r4_shape *s, r4_family f, double *w, double *h);

/* Resolves family f for a box of w x h px (0: unknown), validated and reduced to what GTK 4 CSS
 * border-radius can draw. The downgrade ladder is documented in the README. */
void r4_design_geometry(const r4_design *d,
                        const r4_shape *s,
                        r4_family f,
                        double w,
                        double h,
                        r4_geometry *out,
                        const r4_diag *dg);

/* "4px", or "4px 0px 4px 0px" (TL TR BR BL) when the corners differ. Free with g_free. */
char *r4_geometry_css(const r4_geometry *g);

/* The resolved design, one "key=value" per line, sorted. Free with g_free. */
char *r4_design_dump(const r4_design *d, const r4_shape *s);

/* One line per family: what was asked and what GTK 4 draws. Free with g_free. */
char *r4_design_fidelity(const r4_design *d, const r4_shape *s, const r4_diag *dg);

#endif
