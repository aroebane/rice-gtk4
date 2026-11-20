/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "design.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef R4_DESIGN_DIR
#define R4_DESIGN_DIR "/usr/local/share/rice-gtk4/designs"
#endif

static void note(GPtrArray *a, const char *fmt, ...) G_GNUC_PRINTF(2, 3);
static void note(GPtrArray *a, const char *fmt, ...)
{
    if (!a)
        return;
    va_list ap;
    va_start(ap, fmt);
    char *line = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    /* The stylesheet asks for a family once per rule that uses it: say it once. */
    for (guint i = 0; i < a->len; ++i)
        if (strcmp(a->pdata[i], line) == 0) {
            g_free(line);
            return;
        }
    g_ptr_array_add(a, line);
}

void r4_design_init(r4_design *d)
{
    memset(d, 0, sizeof *d);
    d->caps = (r4_caps){TRUE, FALSE};
    d->des = ricedes_new();
}

void r4_design_clear(r4_design *d)
{
    ricedes_free(d->des);
    memset(d, 0, sizeof *d);
}

/* Moves what rice-design has to say about the input into the theme's lists. */
static void drain(const r4_design *d, const r4_diag *dg)
{
    GPtrArray *to[RICEDES_D_COUNT] = {
        dg ? dg->problems : NULL, dg ? dg->unknown : NULL, dg ? dg->clamps : NULL};
    for (int k = 0; k < RICEDES_D_COUNT; ++k)
        for (size_t i = 0; i < ricedes_diag_count(d->des, (ricedes_diag_t)k); ++i)
            note(to[k], "%s", ricedes_diag_at(d->des, (ricedes_diag_t)k, i));
}

void r4_design_apply_config(r4_design *d, const riceini_t *cfg, const char *file, const r4_diag *dg)
{
    ricedes_apply_config(d->des, cfg, file);
    drain(d, dg);
}

static void take_metrics(const r4_design *d, r4_shape *s)
{
    int v;
    int *const dst[RICEDES_METRIC_COUNT] = {
        [RICEDES_M_BORDER_WIDTH] = &s->border_width,
        [RICEDES_M_SPACING] = &s->spacing,
        [RICEDES_M_PADDING] = &s->padding,
        [RICEDES_M_CONTROL_HEIGHT] = &s->control_height,
        [RICEDES_M_SCROLLBAR_WIDTH] = &s->scrollbar_width,
        [RICEDES_M_INDICATOR_SIZE] = &s->indicator_size,
        [RICEDES_M_FOCUS_WIDTH] = &s->focus_width,
        [RICEDES_M_FOCUS_OFFSET] = &s->focus_offset,
    };
    for (int m = 0; m < RICEDES_METRIC_COUNT; ++m)
        if (ricedes_metric(d->des, (ricedes_metric_t)m, &v))
            *dst[m] = v;
}

void r4_design_load(
    r4_design *d, r4_shape *s, const riceini_t *cfg, const char *cfg_path, const r4_diag *dg)
{
    const char *envp = g_getenv("RICE_GTK4_DESIGN_PATH"), *envn = g_getenv("RICE_GTK4_DESIGN");
    const char *cp = riceini_get(cfg, "Design", "Path"), *cn = riceini_get(cfg, "Design", "Name");
    const char *name = NULL;
    char *path = NULL;
    char *cd = g_path_get_dirname(cfg_path);
    if (envp && *envp)
        path = g_strdup(envp);
    else if (envn && *envn)
        name = envn;
    else if (cp && *cp)
        path = g_path_is_absolute(cp) ? g_strdup(cp) : g_build_filename(cd, cp, NULL);
    else if (cn && *cn)
        name = cn;
    if (path || name) {
        const char *e = g_getenv("RICE_GTK4_DESIGN_DIR");
        char **env_dirs = (e && *e) ? g_strsplit(e, ":", -1) : NULL;
        char *own = g_build_filename(cd, "designs", NULL);
        GPtrArray *dirs = g_ptr_array_new();
        for (guint i = 0; env_dirs && env_dirs[i]; ++i)
            g_ptr_array_add(dirs, env_dirs[i]);
        g_ptr_array_add(dirs, own);
        g_ptr_array_add(dirs, (gpointer)R4_DESIGN_DIR);
        g_ptr_array_add(dirs, NULL);
        ricedes_load(d->des, path ? path : name, path != NULL, (const char *const *)dirs->pdata);
        g_ptr_array_free(dirs, TRUE);
        g_strfreev(env_dirs);
        g_free(own);
        take_metrics(d, s);
    }
    g_free(path);
    g_free(cd);
    drain(d, dg);
}

/* ---------------------------------------------------------------- geometry */

void r4_family_box(const r4_shape *s, r4_family f, double *w, double *h)
{
    int sw_h = s->indicator_size + 2;
    int sc = s->scrollbar_width + 2 * (s->scrollbar_width / 2);
    *w = *h = 0;
    switch (f) {
    case R4_FAM_CHECK:
    case R4_FAM_RADIO:
    case R4_FAM_INDICATOR:
        *w = *h = s->indicator_size;
        break;
    case R4_FAM_SWITCH:
        *w = 2 * sw_h;
        *h = sw_h;
        break;
    case R4_FAM_SLIDER:
        *w = *h = s->indicator_size;
        break;
    case R4_FAM_SCROLLBAR:
        *w = *h = sc;
        break;
    default:
        break;
    }
}

static const char *const k_ladder_why[] = {
    "", "", "", "GTK 4 CSS has no corner chamfer", "GTK 4 CSS has no corner cut", ""};

void r4_design_geometry(const r4_design *d,
                        const r4_shape *s,
                        r4_family f,
                        double w,
                        double h,
                        r4_geometry *out,
                        const r4_diag *dg)
{
    ricedes_corners_t ask;
    /* The config's own [Shape] Radius counts when it names one, or when no design is in use. */
    double legacy = (s->radius_set || !ricedes_name(d->des)) ? s->radius : -1;
    double side = (w > 0 && h > 0) ? fmin(w, h) : (h > 0 ? h : w);
    ricedes_resolve(d->des, f, legacy, w, h, &ask);
    /* GTK scales all four corners by one factor when two neighbours do not fit the box.
     * R4_FULL_RADIUS is therefore a pill only when every corner uses it; next to a small corner it
     * would shrink that corner. */
    int caps_n = 0;
    for (int c = 0; c < 4; ++c)
        caps_n += ask.treatment[c] == R4_TR_CAPSULE;
    gboolean lone_unboxed = side <= 0 && caps_n > 0 && caps_n < 4;
    for (int c = 0; c < 4; ++c) {
        r4_treatment t = ask.treatment[c];
        double size = ask.size[c];
        out->asked[c] = t;
        out->asked_size[c] = size;
        r4_treatment drawn = t;
        double px = size;
        if (t == R4_TR_SHARP) {
            px = 0;
        } else if (t == R4_TR_CAPSULE) {
            drawn = R4_TR_ROUNDED;
            px = side > 0 ? side / 2.0 : R4_FULL_RADIUS;
            if (lone_unboxed) {
                px = ricedes_slot(d->des, 6);
                note(dg ? dg->downgrades : NULL,
                     "%s: a capsule corner next to other corners in a box of unknown size "
                     "drawn as the largest radius of the scale (%g px)",
                     ricedes_family_name(f),
                     px);
            }
        } else if (t == R4_TR_CHAMFERED || t == R4_TR_CUT) {
            drawn = size > 0 ? R4_TR_ROUNDED : R4_TR_SHARP;
            note(dg ? dg->downgrades : NULL,
                 "%s: %s %g px drawn as %s (%s)",
                 ricedes_family_name(f),
                 ricedes_treatment_name(t),
                 size,
                 size > 0 ? "a rounded corner of the same size" : "a sharp corner",
                 k_ladder_why[t]);
        }
        if (drawn == R4_TR_ROUNDED && side > 0 && px > side / 2.0 && px != R4_FULL_RADIUS) {
            note(dg ? dg->clamps : NULL,
                 "%s: radius %g px moved to %g px (half of the %g px box)",
                 ricedes_family_name(f),
                 px,
                 side / 2.0,
                 side);
            px = side / 2.0;
        }
        if (!d->caps.fractional_radius && drawn == R4_TR_ROUNDED && px != R4_FULL_RADIUS &&
            px != floor(px)) {
            note(dg ? dg->downgrades : NULL,
                 "%s: radius %g px drawn as %g px (this toolkit takes whole px)",
                 ricedes_family_name(f),
                 px,
                 floor(px + 0.5));
            px = floor(px + 0.5);
        }
        out->drawn[c] = drawn;
        out->radius[c] = px;
    }
}

static char *num(double v)
{
    char b[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(b, sizeof b, "%.2f", v);
    size_t n = strlen(b);
    while (n > 1 && b[n - 1] == '0')
        b[--n] = 0;
    if (n > 1 && b[n - 1] == '.')
        b[--n] = 0;
    return g_strdup(b);
}

char *r4_geometry_css(const r4_geometry *g)
{
    gboolean same = g->radius[0] == g->radius[1] && g->radius[1] == g->radius[2] &&
                    g->radius[2] == g->radius[3];
    GString *o = g_string_new(NULL);
    for (int c = 0; c < (same ? 1 : 4); ++c) {
        char *n = num(g->radius[c]);
        g_string_append_printf(o, "%s%spx", c ? " " : "", n);
        g_free(n);
    }
    return g_string_free(o, FALSE);
}

/* ---------------------------------------------------------------- dump + fidelity */

static void add_kv(GPtrArray *a, const char *k, const char *fmt, ...) G_GNUC_PRINTF(3, 4);
static void add_kv(GPtrArray *a, const char *k, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *v = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    g_ptr_array_add(a, g_strdup_printf("%s=%s", k, v));
    g_free(v);
}

static gint cmp_str(gconstpointer a, gconstpointer b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

char *r4_design_dump(const r4_design *d, const r4_shape *s)
{
    GPtrArray *a = g_ptr_array_new_with_free_func(g_free);
    add_kv(a, "design.name", "%s", ricedes_name(d->des) ? ricedes_name(d->des) : "(none)");
    add_kv(a, "design.family", "%s", ricedes_preset_name(ricedes_preset(d->des)));
    for (guint i = 0; i < ricedes_chain_count(d->des); ++i) {
        char *k = g_strdup_printf("design.chain.%u", i);
        add_kv(a, k, "%s", ricedes_chain_at(d->des, i));
        g_free(k);
    }
    add_kv(a, "caps.fractional_border", "%d", d->caps.fractional_border);
    add_kv(a, "caps.fractional_radius", "%d", d->caps.fractional_radius);
    add_kv(a, "metrics.border", "%d", s->border_width);
    add_kv(a, "metrics.focus_width", "%d", s->focus_width);
    add_kv(a, "metrics.focus_offset", "%d", s->focus_offset);
    add_kv(a, "metrics.spacing", "%d", s->spacing);
    add_kv(a, "metrics.padding", "%d", s->padding);
    add_kv(a, "metrics.control_height", "%d", s->control_height);
    add_kv(a, "metrics.scrollbar_width", "%d", s->scrollbar_width);
    add_kv(a, "metrics.indicator_size", "%d", s->indicator_size);
    char *rn = num(s->radius);
    add_kv(a, "legacy.radius", "%s%s", rn, s->radius_set ? " (set)" : "");
    g_free(rn);
    for (int i = 0; i < 7; ++i) {
        char *k = g_strdup_printf("radius.%s", ricedes_slot_name(i)),
             *n = num(ricedes_slot(d->des, i));
        add_kv(a, k, "%s", n);
        g_free(k);
        g_free(n);
    }
    for (int f = 0; f < R4_FAM_COUNT; ++f) {
        double w, h;
        r4_geometry g;
        r4_family_box(s, (r4_family)f, &w, &h);
        r4_design_geometry(d, s, (r4_family)f, w, h, &g, NULL);
        char *css = r4_geometry_css(&g),
             *k = g_strdup_printf("shape.%s", ricedes_family_name((r4_family)f));
        add_kv(a, k, "%s %s", ricedes_treatment_name(g.asked[0]), css);
        g_free(k);
        g_free(css);
    }
    g_ptr_array_sort(a, cmp_str);
    g_ptr_array_add(a, NULL);
    char *joined = g_strjoinv("\n", (char **)a->pdata);
    g_ptr_array_set_free_func(a, g_free);
    g_ptr_array_free(a, TRUE);
    char *out = g_strconcat(joined, "\n", NULL);
    g_free(joined);
    return out;
}

char *r4_design_fidelity(const r4_design *d, const r4_shape *s, const r4_diag *dg)
{
    GString *o = g_string_new(NULL);
    g_string_append_printf(o,
                           "design: %s\n",
                           ricedes_name(d->des) ? ricedes_name(d->des)
                                                : "(none: legacy [Shape] Radius)");
    for (int f = 0; f < R4_FAM_COUNT; ++f) {
        double w, h;
        r4_geometry g;
        r4_family_box(s, (r4_family)f, &w, &h);
        r4_design_geometry(d, s, (r4_family)f, w, h, &g, NULL);
        if (f == R4_FAM_FOCUS) {
            /* GTK 4 has no outline-radius: the ring takes the widget's corners and grows them by
             * offset + width. */
            g_string_append_printf(o,
                                   "  %-10s %-9s -> follows the widget's own corners\n",
                                   ricedes_family_name((r4_family)f),
                                   ricedes_treatment_name(g.asked[0]));
            continue;
        }
        gboolean exact = TRUE;
        for (int c = 0; c < 4; ++c)
            exact = exact && (g.asked[c] == g.drawn[c] ||
                              (g.asked[c] == R4_TR_CAPSULE && g.drawn[c] == R4_TR_ROUNDED));
        char *css = r4_geometry_css(&g);
        g_string_append_printf(o,
                               "  %-10s %-9s -> %s %s\n",
                               ricedes_family_name((r4_family)f),
                               ricedes_treatment_name(g.asked[0]),
                               exact ? "exact  " : "reduced",
                               css);
        g_free(css);
    }
    const struct {
        const char *title;
        GPtrArray *a;
    } sec[] = {{"downgrades", dg ? dg->downgrades : NULL},
               {"clamps", dg ? dg->clamps : NULL},
               {"unknown keys", dg ? dg->unknown : NULL},
               {"problems", dg ? dg->problems : NULL}};
    for (guint i = 0; i < G_N_ELEMENTS(sec); ++i) {
        if (!sec[i].a || !sec[i].a->len)
            continue;
        g_string_append_printf(o, "%s:\n", sec[i].title);
        for (guint j = 0; j < sec[i].a->len; ++j)
            g_string_append_printf(o, "  %s\n", (char *)sec[i].a->pdata[j]);
    }
    return g_string_free(o, FALSE);
}
