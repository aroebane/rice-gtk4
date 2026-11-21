/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "theme.h"

#include <rice-ini/rice-ini.h>
#include <stdlib.h>
#include <string.h>

#include "elements.h"

static r4_color rgb(uint32_t v)
{
    return ricecol_from_rgb8(v);
}

r4_color r4_mix(r4_color a, r4_color b, double bias)
{
    return ricecol_mix(a, b, bias);
}
double r4_contrast(r4_color a, r4_color b)
{
    return ricecol_contrast_ratio(a, b);
}

r4_color r4_readable_on(r4_color fill, r4_color dark, r4_color light)
{
    return ricecol_contrast_ratio(fill, dark) >= ricecol_contrast_ratio(fill, light) ? dark : light;
}

r4_color r4_hover(const r4_theme *t, r4_color fill)
{
    return r4_mix(fill, t->text, t->knobs.hover_layer);
}
r4_color r4_pressed(const r4_theme *t, r4_color fill)
{
    return r4_mix(fill, t->text, t->knobs.pressed_layer);
}

/* A disabled container is the text colour at disabled_container over the window.
 * The fill passed in is unused; every disabled surface reads the same. */
r4_color r4_disabled_fill(const r4_theme *t, r4_color fill)
{
    (void)fill;
    return r4_mix(t->window, t->text, t->knobs.disabled_container);
}

r4_color r4_disabled_text(const r4_theme *t, r4_color text)
{
    return r4_mix(t->window, text, t->knobs.disabled_content);
}

void r4_hex(r4_color c, char buf[8])
{
    uint32_t v = ricecol_to_argb8(c);
    g_snprintf(buf, 8, "#%02x%02x%02x", (v >> 16U) & 255U, (v >> 8U) & 255U, v & 255U);
}

char *r4_config_path(void)
{
    const char *e = g_getenv("RICE_GTK4_CONFIG");
    if (e && *e)
        return g_strdup(e);
    const char *x = g_getenv("XDG_CONFIG_HOME");
    if (x && *x)
        return g_build_filename(x, "rice", "gtk4.conf", NULL);
    return g_build_filename(g_get_home_dir(), ".config", "rice", "gtk4.conf", NULL);
}

static void problem(r4_theme *t, const char *section, const char *key, const char *why)
{
    g_ptr_array_add(t->problems, g_strdup_printf("[%s] %s: %s", section, key, why));
}

/* The keys each section reads: anything else is reported as unknown. */
static const char *const k_sections[] = {"Source",
                                         "Shape",
                                         "Colors",
                                         "Style",
                                         "Fonts",
                                         "Icons",
                                         "Palette",
                                         "Control",
                                         "Theme",
                                         "State",
                                         "Contrast",
                                         "Focus",
                                         "Selection",
                                         "Motion",
                                         NULL};
static const char *const k_keys[] = {"Source:Path",
                                     "Shape:Radius",
                                     "Shape:BorderWidth",
                                     "Shape:FocusWidth",
                                     "Shape:FocusOffset",
                                     "Shape:Spacing",
                                     "Shape:Padding",
                                     "Shape:ControlHeight",
                                     "Shape:ScrollBarWidth",
                                     "Shape:IndicatorSize",
                                     "Colors:Window",
                                     "Colors:Text",
                                     "Colors:Base",
                                     "Colors:Button",
                                     "Colors:Accent",
                                     "Colors:Border",
                                     "Colors:Focus",
                                     "Colors:Container",
                                     "Colors:ContainerText",
                                     "Colors:Primary",
                                     "Colors:PrimaryText",
                                     "Theme:Mode",
                                     "Style:ColorScheme",
                                     "Fonts:General",
                                     "Icons:Theme",
                                     "State:HoverLayer",
                                     "State:PressedLayer",
                                     "State:DisabledContent",
                                     "State:DisabledContainer",
                                     "Contrast:Text",
                                     "Contrast:Boundary",
                                     "Contrast:Enforce",
                                     "Focus:Mode",
                                     "Selection:Mode",
                                     "Selection:Boundary",
                                     "Motion:Enabled",
                                     "Motion:Duration",
                                     "State:StatusTint",
                                     "State:PointerHover",
                                     "Focus:Visible",
                                     NULL};

static gboolean known(const char *section, const char *key)
{
    if (!strcmp(section, "Palette") || !strcmp(section, "Control"))
        return TRUE;
    /* The design layer reports its own sections, with the file name. */
    if (!strcmp(section, "Design") || !strncmp(section, "Shape.", 6))
        return TRUE;
    /* [Emphasis] and [Element.<id>] are checked key by key in read_overrides(). */
    if (!strcmp(section, "Emphasis") || !strncmp(section, "Element.", 8))
        return TRUE;
    char *id = g_strdup_printf("%s:%s", section, key);
    gboolean ok = FALSE;
    for (int i = 0; k_keys[i] && !ok; ++i)
        ok = !strcmp(k_keys[i], id);
    g_free(id);
    return ok;
}

static gboolean get_color(const riceini_t *ini, const char *s, const char *k, r4_color *out)
{
    const char *v = riceini_get(ini, s, k);
    uint8_t c[4];
    if (!v || !*v || !riceini_to_color(v, c))
        return FALSE;
    *out = ricecol_from_argb8(((uint32_t)c[3] << 24U) | ((uint32_t)c[0] << 16U) |
                              ((uint32_t)c[1] << 8U) | c[2]);
    return TRUE;
}

static void get_int(r4_theme *t, const riceini_t *ini, const char *k, int *out, int lo, int hi)
{
    const char *v = riceini_get(ini, "Shape", k);
    int64_t n;
    if (!v || !*v)
        return;
    if (!riceini_to_int(v, &n) || n < lo || n > hi) {
        problem(t, "Shape", k, "not an integer in range");
        return;
    }
    *out = (int)n;
}

static void defaults(r4_theme *t)
{
    memset(t, 0, sizeof *t);
    t->window = rgb(0x1e1e2e);
    t->text = rgb(0xcdd6f4);
    t->base = rgb(0x181825);
    t->button = rgb(0x313244);
    t->accent = rgb(0xb4befe);
    t->border = rgb(0x6c7086);
    t->focus = rgb(0xb4befe);
    t->muted = rgb(0xa6adc8);
    t->positive = rgb(0xa6e3a1);
    t->warning = rgb(0xf9e2af);
    t->negative = rgb(0xf38ba8);
    t->surface_variant = rgb(0x45475a);
    t->scrim = rgb(0x000000);
    t->shape = (r4_shape){0, FALSE, 1, 2, 1, 8, 8, 28, 12, 16};
    t->knobs = (r4_knobs){0.08,
                          0.14,
                          0.38,
                          0.12,
                          4.5,
                          3.0,
                          TRUE,
                          R4_FOCUS_INSET,
                          FALSE,
                          TRUE,
                          TRUE,
                          80,
                          0.16,
                          TRUE,
                          FALSE};
    t->dark = TRUE;
}

static const double k_tint = 0.22, k_text_tint = 0.55, k_text_contrast = 4.5;

static const char *const k_ctl_names[R4_CTL_COUNT] = {"selection",
                                                      "selectionText",
                                                      "track",
                                                      "trackHover",
                                                      "thumb",
                                                      "thumbHover",
                                                      "thumbActive",
                                                      "visited",
                                                      "tabText",
                                                      "disabledStrong",
                                                      "borderSubtle",
                                                      "borderRule",
                                                      "borderRestingLink",
                                                      "hoverSurface",
                                                      "pressedSurface",
                                                      "containerHover",
                                                      "containerPressed",
                                                      "primaryHover",
                                                      "primaryPressed",
                                                      "disabledText"};

/* The palette file's [Control.<emphasis>] sections, their keys and the role names they may hold, in
 * r4_paint_emph, r4_paint_slot and r4_role order. */
static const char *const k_paint_sections[R4_PE_COUNT] = {
    "Control.outline", "Control.flat", "Control.solid"};
static const char *const k_paint_keys[R4_PS_COUNT] = {"restBorder",
                                                      "hoverBorder",
                                                      "pressedFill",
                                                      "pressedBorder",
                                                      "selectFill",
                                                      "selectText",
                                                      "selectBorder",
                                                      "disabledFill",
                                                      "disabledBorder"};
static const char *const k_role_names[R4_ROLE_COUNT] = {NULL,
                                                        "none",
                                                        "surface",
                                                        "hoverSurface",
                                                        "focusSurface",
                                                        "pressedSurface",
                                                        "container",
                                                        "containerHover",
                                                        "containerPressed",
                                                        "currentContainer",
                                                        "disabledSurface",
                                                        "primary",
                                                        "primaryHover",
                                                        "primaryFocus",
                                                        "primaryPressed",
                                                        "text",
                                                        "muted",
                                                        "containerText",
                                                        "currentText",
                                                        "disabledText",
                                                        "primaryText",
                                                        "outline",
                                                        "focus",
                                                        "hover",
                                                        "current",
                                                        "disabledBorder"};

/* What the producer says one emphasis paints in one state: a role name, resolved by css.c. */
static void read_paint(r4_theme *t, const riceini_t *pal)
{
    for (int e = 0; e < R4_PE_COUNT; ++e)
        for (int s = 0; s < R4_PS_COUNT; ++s) {
            const char *v = riceini_get(pal, k_paint_sections[e], k_paint_keys[s]);
            if (!v || !*v)
                continue;
            for (int r = R4_ROLE_NONE; r < R4_ROLE_COUNT; ++r)
                if (!strcmp(v, k_role_names[r]))
                    t->paint[e][s] = (r4_role)r;
            if (t->paint[e][s] == R4_ROLE_UNSET)
                problem(t, k_paint_sections[e], k_paint_keys[s], "unknown role name");
        }
}

static void set_ctl(r4_theme *t, r4_ctl i, r4_color v)
{
    t->ctl[i] = v;
    t->ctl_set |= 1U << (unsigned)i;
}

/* A colour the config or the contrast guard changed after the producer computed its state colours
 * from the old one: the producer's states no longer fit it, so the engine derives them again. */
static void drop_ctl(r4_theme *t, unsigned mask)
{
    t->ctl_set &= ~mask;
}
#define R4_BIT(i) (1U << (unsigned)(i))
#define R4_PRIMARY_STATES (R4_BIT(R4_CTL_PRIMARY_HOVER) | R4_BIT(R4_CTL_PRIMARY_PRESSED))
#define R4_CONTAINER_STATES (R4_BIT(R4_CTL_CONTAINER_HOVER) | R4_BIT(R4_CTL_CONTAINER_PRESSED))
#define R4_BASE_STATES (R4_BIT(R4_CTL_HOVER_SURFACE) | R4_BIT(R4_CTL_PRESSED_SURFACE))

static void get_real(r4_theme *t,
                     const riceini_t *ini,
                     const char *s,
                     const char *k,
                     double *out,
                     double lo,
                     double hi)
{
    const char *v = riceini_get(ini, s, k);
    double d;
    if (!v || !*v)
        return;
    if (!riceini_to_double(v, &d) || d < lo || d > hi) {
        problem(t, s, k, "not a number in range");
        return;
    }
    *out = d;
}

static void get_bool(r4_theme *t, const riceini_t *ini, const char *s, const char *k, gboolean *out)
{
    const char *v = riceini_get(ini, s, k);
    if (!v || !*v)
        return;
    if (!strcmp(v, "true") || !strcmp(v, "on") || !strcmp(v, "yes") || !strcmp(v, "1"))
        *out = TRUE;
    else if (!strcmp(v, "false") || !strcmp(v, "off") || !strcmp(v, "no") || !strcmp(v, "0"))
        *out = FALSE;
    else
        problem(t, s, k, "expected true or false");
}

/* [State] [Contrast] [Focus] [Selection] [Motion]: every key optional, every value checked. */
static void read_knobs(r4_theme *t, const riceini_t *cfg)
{
    r4_knobs *k = &t->knobs;
    get_real(t, cfg, "State", "HoverLayer", &k->hover_layer, 0.0, 0.5);
    get_real(t, cfg, "State", "PressedLayer", &k->pressed_layer, 0.0, 0.5);
    get_real(t, cfg, "State", "DisabledContent", &k->disabled_content, 0.0, 1.0);
    get_real(t, cfg, "State", "DisabledContainer", &k->disabled_container, 0.0, 1.0);
    get_real(t, cfg, "State", "StatusTint", &k->status_tint, 0.0, 1.0);
    get_real(t, cfg, "Contrast", "Text", &k->text_contrast, 1.0, 21.0);
    get_real(t, cfg, "Contrast", "Boundary", &k->boundary_contrast, 1.0, 21.0);
    get_bool(t, cfg, "Contrast", "Enforce", &k->enforce);
    get_bool(t, cfg, "Selection", "Boundary", &k->selection_boundary);
    get_bool(t, cfg, "Motion", "Enabled", &k->motion);
    get_bool(t, cfg, "State", "PointerHover", &k->pointer_hover);
    const char *v = riceini_get(cfg, "Focus", "Mode");
    if (v && *v) {
        if (!strcmp(v, "inset"))
            k->focus_mode = R4_FOCUS_INSET;
        else if (!strcmp(v, "outer"))
            k->focus_mode = R4_FOCUS_OUTER;
        else if (!strcmp(v, "border"))
            k->focus_mode = R4_FOCUS_BORDER;
        else
            problem(t, "Focus", "Mode", "expected inset, outer or border");
    }
    v = riceini_get(cfg, "Focus", "Visible");
    if (v && *v) {
        if (!strcmp(v, "keyboard"))
            k->focus_always = FALSE;
        else if (!strcmp(v, "always"))
            k->focus_always = TRUE;
        else
            problem(t, "Focus", "Visible", "expected keyboard or always");
    }
    v = riceini_get(cfg, "Selection", "Mode");
    if (v && *v) {
        if (!strcmp(v, "tonal"))
            k->selection_solid = FALSE;
        else if (!strcmp(v, "solid"))
            k->selection_solid = TRUE;
        else
            problem(t, "Selection", "Mode", "expected tonal or solid");
    }
    v = riceini_get(cfg, "Motion", "Duration");
    if (v && *v) {
        int64_t n;
        if (riceini_to_int(v, &n) && n >= 0 && n <= 1000)
            k->motion_ms = (int)n;
        else
            problem(t, "Motion", "Duration", "not an integer in 0..1000");
    }
}

/* Contrast guard. A role that misses its floor against the surfaces it sits on is moved toward
 * the text colour in 2% steps until it passes (the nearest passing tone, no other theme is
 * chosen). With Enforce=false it is only reported. Always reported, so nothing is silent. */
static r4_color guard_role(r4_theme *t,
                           const char *name,
                           r4_color c,
                           const r4_color *surf,
                           int n,
                           double floor,
                           gboolean *changed)
{
    double worst = 1e9;
    for (int i = 0; i < n; ++i)
        worst = MIN(worst, r4_contrast(c, surf[i]));
    if (worst >= floor)
        return c;
    r4_color out = c;
    double now = worst;
    for (int step = 1; step <= 50 && now < floor; ++step) {
        out = r4_mix(c, t->text, step * 0.02);
        now = 1e9;
        for (int i = 0; i < n; ++i)
            now = MIN(now, r4_contrast(out, surf[i]));
    }
    g_ptr_array_add(t->corrections,
                    g_strdup_printf("%s %s: %.2f:1 is below %.2f:1%s",
                                    name,
                                    t->knobs.enforce ? "raised" : "fails",
                                    worst,
                                    floor,
                                    t->knobs.enforce ? "" : " (Enforce=false, left as is)"));
    if (!t->knobs.enforce)
        return c;
    *changed = TRUE;
    return out;
}

/* The text on a tinted status fill must reach the text floor for all four status colours; if the
 * configured tint is too strong for this palette, the tint is lowered (or only reported). */
static void guard_status_tint(r4_theme *t)
{
    const r4_color st[4] = {t->accent, t->positive, t->warning, t->negative};
    const double want = t->knobs.status_tint, floor = t->knobs.text_contrast;
    double tint = want;
    for (;;) {
        gboolean ok = TRUE;
        for (int i = 0; i < 4 && ok; ++i) {
            r4_color fill = r4_mix(t->base, st[i], tint);
            ok = r4_contrast(r4_readable_on(fill, t->window, t->text), fill) >= floor;
        }
        if (ok || tint <= 0.0)
            break;
        tint = MAX(0.0, tint - 0.02);
    }
    if (tint >= want)
        return;
    g_ptr_array_add(
        t->corrections,
        g_strdup_printf("StatusTint %s: %.2f gives text below %.2f:1 on a status fill, %.2f does%s",
                        t->knobs.enforce ? "lowered" : "fails",
                        want,
                        floor,
                        tint,
                        t->knobs.enforce ? "" : " (Enforce=false, left as is)"));
    if (t->knobs.enforce)
        t->knobs.status_tint = tint;
}

static void guard(r4_theme *t)
{
    const r4_color surf[2] = {t->window, t->base};
    gboolean changed = FALSE, primary_changed = FALSE;
    const double b = t->knobs.boundary_contrast, x = t->knobs.text_contrast;
    t->border = guard_role(t, "border", t->border, surf, 2, b, &changed);
    t->focus = guard_role(t, "focus", t->focus, surf, 2, b, &changed);
    t->accent = guard_role(t, "accent", t->accent, surf, 2, b, &changed);
    t->primary = guard_role(t, "primary", t->primary, surf, 2, b, &primary_changed);
    t->muted = guard_role(t, "muted", t->muted, surf, 2, x, &changed);
    if (primary_changed) {
        t->primary_text = r4_readable_on(t->primary, t->window, t->text);
        drop_ctl(t, R4_PRIMARY_STATES);
    }
    guard_status_tint(t);
}

static void derive(r4_theme *t, const riceini_t *pal, const riceini_t *cfg)
{
    r4_color v, win_text;
    /* The thirteen palette roles; absent ones keep the built-in value. */
    if (get_color(pal, "Palette", "background", &v))
        t->window = v;
    if (get_color(pal, "Palette", "text", &v))
        t->text = v;
    if (get_color(pal, "Palette", "surface", &v))
        t->base = v;
    if (get_color(pal, "Palette", "elevated", &v))
        t->button = v;
    if (get_color(pal, "Palette", "accent", &v))
        t->accent = v;
    if (get_color(pal, "Palette", "outline", &v))
        t->border = v;
    if (get_color(pal, "Palette", "focus", &v))
        t->focus = v;
    if (get_color(pal, "Palette", "textMuted", &v))
        t->muted = v;
    if (get_color(pal, "Palette", "positive", &v))
        t->positive = v;
    if (get_color(pal, "Palette", "warning", &v))
        t->warning = v;
    if (get_color(pal, "Palette", "surfaceVariant", &v))
        t->surface_variant = v;
    if (get_color(pal, "Palette", "scrim", &v))
        t->scrim = v;
    if (get_color(pal, "Palette", "selection", &v))
        set_ctl(t, R4_CTL_SELECTION, v);
    if (get_color(pal, "Palette", "negative", &v))
        t->negative = v;
    /* Derived roles from the producer win over the engine's own derivation. */
    if (get_color(pal, "Control", "text", &v))
        t->text = v;
    if (get_color(pal, "Control", "outline", &v))
        t->border = v;
    if (get_color(pal, "Control", "focus", &v))
        t->focus = v;
    if (get_color(pal, "Control", "muted", &v))
        t->muted = v;
    for (int i = 0; i < R4_CTL_COUNT; ++i)
        if (get_color(pal, "Control", k_ctl_names[i], &v))
            set_ctl(t, (r4_ctl)i, v);
    read_paint(t, pal);
    /* [Colors] in the config overrides key by key. */
    if (get_color(cfg, "Colors", "Window", &v))
        t->window = v;
    if (get_color(cfg, "Colors", "Text", &v))
        t->text = v;
    if (get_color(cfg, "Colors", "Base", &v)) {
        t->base = v;
        drop_ctl(t, R4_BASE_STATES);
    }
    if (get_color(cfg, "Colors", "Button", &v))
        t->button = v;
    if (get_color(cfg, "Colors", "Accent", &v))
        t->accent = v;
    if (get_color(cfg, "Colors", "Border", &v))
        t->border = v;
    if (get_color(cfg, "Colors", "Focus", &v))
        t->focus = v;

    win_text = t->text;
    t->container = r4_mix(t->window, t->accent, k_tint);
    r4_color on = r4_mix(win_text, t->accent, k_text_tint);
    if (r4_contrast(on, t->container) < k_text_contrast)
        on = win_text;
    if (r4_contrast(on, t->container) < k_text_contrast)
        on = r4_readable_on(t->container, t->window, t->text);
    t->container_text = on;
    t->primary = t->accent;
    t->primary_text = r4_readable_on(t->primary, t->window, t->text);
    if (get_color(pal, "Control", "container", &v))
        t->container = v;
    if (get_color(pal, "Control", "containerText", &v))
        t->container_text = v;
    if (get_color(pal, "Control", "primary", &v)) {
        t->primary = v;
        t->primary_text = r4_readable_on(v, t->window, t->text);
    }
    if (get_color(pal, "Control", "primaryText", &v))
        t->primary_text = v;
    if (get_color(cfg, "Colors", "Container", &v)) {
        t->container = v;
        drop_ctl(t, R4_CONTAINER_STATES);
    }
    if (get_color(cfg, "Colors", "ContainerText", &v))
        t->container_text = v;
    if (get_color(cfg, "Colors", "Primary", &v)) {
        drop_ctl(t, R4_PRIMARY_STATES);
        t->primary = v;
        t->primary_text = r4_readable_on(v, t->window, t->text);
    }
    if (get_color(cfg, "Colors", "PrimaryText", &v))
        t->primary_text = v;
}

/* ---- [Emphasis] and [Element.<id>] ------------------------------------------------- */

static const char *const k_emph_keys[R4_EF_COUNT] = {"Button", "Input", "Tab", "Row", "MenuItem"};

/* Colour names an override may use: the @define-color names the stylesheet defines. */
static const char *const k_paint_roles[] = {
    "window",         "text",     "base",    "button",        "elevated",       "accent",
    "border",         "focus",    "muted",   "container",     "container_text", "primary",
    "primary_text",   "positive", "warning", "negative",      "info",           "selection",
    "selection_text", "track",    "thumb",   "border_subtle", "border_rule",    NULL};

static gboolean parse_emphasis(const char *v, r4_emphasis *out)
{
    if (!strcmp(v, "outline"))
        *out = R4_EMPH_OUTLINE;
    else if (!strcmp(v, "flat"))
        *out = R4_EMPH_FLAT;
    else if (!strcmp(v, "solid"))
        *out = R4_EMPH_SOLID;
    else if (!strcmp(v, "default"))
        *out = R4_EMPH_DEFAULT;
    else
        return FALSE;
    return TRUE;
}

/* A role name, `transparent`/`none`, or a hex colour, as CSS text; NULL if it is none of these. */
static char *parse_paint(const char *v)
{
    if (!strcmp(v, "none") || !strcmp(v, "transparent"))
        return g_strdup("transparent");
    const char *role = (*v == '@') ? v + 1 : v; /* `@primary` and `primary` both name a role */
    for (int i = 0; k_paint_roles[i]; ++i)
        if (!strcmp(role, k_paint_roles[i]))
            return g_strdup_printf("@%s", role);
    uint8_t c[4];
    if (*v != '#' || !riceini_to_color(v, c))
        return NULL;
    if (c[3] == 255)
        return g_strdup_printf("#%02x%02x%02x", c[0], c[1], c[2]);
    char a[G_ASCII_DTOSTR_BUF_SIZE];
    (void)g_ascii_formatd(a, sizeof a, "%.3f", c[3] / 255.0);
    return g_strdup_printf("rgba(%u, %u, %u, %s)", c[0], c[1], c[2], a);
}

static void element_int(
    r4_theme *t, const riceini_t *cfg, const char *sec, const char *key, int *out, int lo, int hi)
{
    const char *v = riceini_get(cfg, sec, key);
    int64_t n;
    if (!v || !*v)
        return;
    if (riceini_to_int(v, &n) && n >= lo && n <= hi)
        *out = (int)n;
    else {
        char *why = g_strdup_printf("not an integer in %d..%d", lo, hi);
        problem(t, sec, key, why);
        g_free(why);
    }
}

static void
element_paint(r4_theme *t, const riceini_t *cfg, const char *sec, const char *key, char **out)
{
    const char *v = riceini_get(cfg, sec, key);
    if (!v || !*v)
        return;
    char *p = parse_paint(v);
    if (p) {
        g_free(*out);
        *out = p;
    } else
        problem(t, sec, key, "expected a role name, transparent or #rrggbb[aa]");
}

void r4_element_over_free(gpointer p)
{
    r4_element_over *e = p;
    g_free(e->id);
    g_free(e->fill);
    g_free(e->text);
    g_free(e->border);
    g_free(e);
}

static const char *const k_element_keys[] = {"Fill",
                                             "Text",
                                             "Border",
                                             "Radius",
                                             "BorderWidth",
                                             "PaddingX",
                                             "PaddingY",
                                             "MinHeight",
                                             "Emphasis",
                                             NULL};

static void read_overrides(r4_theme *t, const riceini_t *cfg)
{
    for (int f = 0; f < R4_EF_COUNT; ++f) {
        const char *v = riceini_get(cfg, "Emphasis", k_emph_keys[f]);
        if (v && *v && !parse_emphasis(v, &t->emphasis[f]))
            problem(t, "Emphasis", k_emph_keys[f], "expected outline, flat, solid or default");
    }
    for (size_t i = 0; i < riceini_entry_count(cfg); ++i) {
        const char *s = riceini_entry_section(cfg, i), *k = riceini_entry_key(cfg, i);
        if (s && !strcmp(s, "Emphasis")) {
            gboolean ok = FALSE;
            for (int f = 0; f < R4_EF_COUNT && !ok; ++f)
                ok = !strcmp(k_emph_keys[f], k);
            if (!ok)
                g_ptr_array_add(t->unknown, g_strdup_printf("[%s] %s", s, k));
            continue;
        }
        if (!s || strncmp(s, "Element.", 8) != 0)
            continue;
        const char *id = s + 8;
        if (!r4_element_find(id)) {
            /* Reported once per section, on its first key. */
            gboolean first = TRUE;
            for (size_t j = 0; j < i; ++j)
                if (!strcmp(riceini_entry_section(cfg, j), s))
                    first = FALSE;
            if (first)
                problem(t, s, "(section)", "not an element id of element-hooks.yaml");
            continue;
        }
        gboolean ok = FALSE;
        for (int j = 0; k_element_keys[j] && !ok; ++j)
            ok = !strcmp(k_element_keys[j], k);
        if (!ok)
            g_ptr_array_add(t->unknown, g_strdup_printf("[%s] %s", s, k));
        r4_element_over *e = NULL;
        for (guint j = 0; j < t->elements->len && !e; ++j)
            if (!strcmp(((r4_element_over *)t->elements->pdata[j])->id, id))
                e = t->elements->pdata[j];
        if (e)
            continue; /* the section was already read whole */
        e = g_new0(r4_element_over, 1);
        e->id = g_strdup(id);
        e->radius = e->border_width = e->pad_x = e->pad_y = e->min_height = -1;
        element_paint(t, cfg, s, "Fill", &e->fill);
        element_paint(t, cfg, s, "Text", &e->text);
        element_paint(t, cfg, s, "Border", &e->border);
        element_int(t, cfg, s, "Radius", &e->radius, 0, 64);
        element_int(t, cfg, s, "BorderWidth", &e->border_width, 0, 8);
        element_int(t, cfg, s, "PaddingX", &e->pad_x, 0, 64);
        element_int(t, cfg, s, "PaddingY", &e->pad_y, 0, 64);
        element_int(t, cfg, s, "MinHeight", &e->min_height, 0, 256);
        const char *v = riceini_get(cfg, s, "Emphasis");
        if (v && *v && !parse_emphasis(v, &e->emphasis))
            problem(t, s, "Emphasis", "expected outline, flat, solid or default");
        g_ptr_array_add(t->elements, e);
    }
}

/* The colour a validated override paint resolves to; FALSE for what cannot be resolved here
 * (transparent, alpha colours, roles derived at render time). */
static gboolean paint_color(const r4_theme *t, const char *css, r4_color *out)
{
    const struct {
        const char *name;
        r4_color c;
    } roles[] = {{"window", t->window},
                 {"text", t->text},
                 {"base", t->base},
                 {"button", t->button},
                 {"elevated", t->button},
                 {"accent", t->accent},
                 {"info", t->accent},
                 {"border", t->border},
                 {"focus", t->focus},
                 {"muted", t->muted},
                 {"container", t->container},
                 {"container_text", t->container_text},
                 {"primary", t->primary},
                 {"primary_text", t->primary_text},
                 {"positive", t->positive},
                 {"warning", t->warning},
                 {"negative", t->negative}};
    if (*css == '@') {
        for (guint i = 0; i < G_N_ELEMENTS(roles); ++i)
            if (!strcmp(css + 1, roles[i].name)) {
                *out = roles[i].c;
                return TRUE;
            }
        return FALSE;
    }
    uint8_t c[4];
    if (*css != '#' || strlen(css) != 7 || !riceini_to_color(css, c))
        return FALSE;
    *out = rgb(((uint32_t)c[0] << 16U) | ((uint32_t)c[1] << 8U) | (uint32_t)c[2]);
    return TRUE;
}

/* Overrides are held to the same floors as the palette: a Text that misses the text floor on its
 * Fill is replaced by the readable one, a Border that misses the boundary floor is raised. A pair
 * with only one side given, or a side that resolves later (transparent, alpha), is not checked. */
static void guard_overrides(r4_theme *t)
{
    const r4_color surf[2] = {t->window, t->base};
    for (guint i = 0; i < t->elements->len; ++i) {
        r4_element_over *e = t->elements->pdata[i];
        r4_color fill, text, border;
        if (e->fill && e->text && paint_color(t, e->fill, &fill) &&
            paint_color(t, e->text, &text) && r4_contrast(text, fill) < t->knobs.text_contrast) {
            r4_color fixed = r4_readable_on(fill, t->window, t->text);
            g_ptr_array_add(
                t->corrections,
                g_strdup_printf("[Element.%s] Text %s: %.2f:1 on its Fill is below %.2f:1%s",
                                e->id,
                                t->knobs.enforce ? "replaced" : "fails",
                                r4_contrast(text, fill),
                                t->knobs.text_contrast,
                                t->knobs.enforce ? "" : " (Enforce=false, left as is)"));
            if (t->knobs.enforce) {
                char hex[8];
                r4_hex(fixed, hex);
                g_free(e->text);
                e->text = g_strdup(hex);
            }
        }
        if (e->border && paint_color(t, e->border, &border)) {
            gboolean changed = FALSE;
            char *nm = g_strdup_printf("[Element.%s] Border", e->id);
            r4_color fixed =
                guard_role(t, nm, border, surf, 2, t->knobs.boundary_contrast, &changed);
            g_free(nm);
            if (changed) {
                char hex[8];
                r4_hex(fixed, hex);
                g_free(e->border);
                e->border = g_strdup(hex);
            }
        }
    }
}

const r4_element_def *r4_element_find(const char *id)
{
    for (unsigned i = 0; i < r4_element_def_count; ++i)
        if (!strcmp(r4_element_defs[i].id, id))
            return &r4_element_defs[i];
    return NULL;
}

r4_theme *r4_theme_load(const char *path)
{
    r4_theme *t = g_new0(r4_theme, 1);
    defaults(t);
    t->unknown = g_ptr_array_new_with_free_func(g_free);
    t->problems = g_ptr_array_new_with_free_func(g_free);
    t->corrections = g_ptr_array_new_with_free_func(g_free);
    t->elements = g_ptr_array_new_with_free_func(r4_element_over_free);
    t->config_path = path ? g_strdup(path) : r4_config_path();
    r4_design_init(&t->design);
    t->diag = (r4_diag){t->problems,
                        t->unknown,
                        g_ptr_array_new_with_free_func(g_free),
                        g_ptr_array_new_with_free_func(g_free)};

    riceini_t *cfg = riceini_load(t->config_path);
    if (!cfg)
        return t;
    /* The design sets the metrics first; the config's own [Shape] keys below override them. */
    r4_design_load(&t->design, &t->shape, cfg, t->config_path, &t->diag);
    r4_design_apply_config(&t->design, cfg, t->config_path, &t->diag);
    const char *src = riceini_get(cfg, "Source", "Path");
    char *dir = g_path_get_dirname(t->config_path);
    char *ppath = (src && *src)
                      ? (g_path_is_absolute(src) ? g_strdup(src) : g_build_filename(dir, src, NULL))
                      : g_build_filename(dir, "gtk4-palette.ini", NULL);
    riceini_t *pal = riceini_load(ppath);
    if (!pal)
        pal = riceini_parse("", 0);
    read_knobs(t, pal); /* the producer's amounts (the shell's tuning) ... */
    read_knobs(t, cfg); /* ... and gtk4.conf wins, as with the colours */
    read_overrides(t, cfg);
    derive(t, pal, cfg);
    guard(t);
    guard_overrides(t);
    if (t->knobs.selection_solid && !((t->ctl_set >> (unsigned)R4_CTL_SELECTION) & 1U)) {
        t->container = t->primary;
        t->container_text = t->primary_text;
        drop_ctl(t, R4_CONTAINER_STATES);
    }

    get_int(t, cfg, "BorderWidth", &t->shape.border_width, 0, 8);
    get_int(t, cfg, "FocusWidth", &t->shape.focus_width, 0, 8);
    get_int(t, cfg, "FocusOffset", &t->shape.focus_offset, -4, 8);
    get_int(t, cfg, "Spacing", &t->shape.spacing, 0, 64);
    get_int(t, cfg, "Padding", &t->shape.padding, 0, 64);
    get_int(t, cfg, "ControlHeight", &t->shape.control_height, 12, 128);
    get_int(t, cfg, "ScrollBarWidth", &t->shape.scrollbar_width, 4, 64);
    get_int(t, cfg, "IndicatorSize", &t->shape.indicator_size, 8, 64);
    const char *r = riceini_get(cfg, "Shape", "Radius");
    double d;
    if (r && *r) {
        if (riceini_to_double(r, &d) && d >= 0 && d <= 64) {
            t->shape.radius = d;
            t->shape.radius_set = TRUE;
        } else
            problem(t, "Shape", "Radius", "not a number in range");
    }
    const char *mode = riceini_get(pal, "Theme", "Mode");
    if (mode && *mode) {
        if (!strcmp(mode, "dark"))
            t->dark = TRUE;
        else if (!strcmp(mode, "light"))
            t->dark = FALSE;
        else
            problem(t, "Theme", "Mode", "expected dark or light");
    }
    const char *cs = riceini_get(cfg, "Style", "ColorScheme");
    if (cs && *cs) {
        if (!strcmp(cs, "dark"))
            t->dark = TRUE;
        else if (!strcmp(cs, "light"))
            t->dark = FALSE;
        else
            problem(t, "Style", "ColorScheme", "expected dark or light");
    }
    const char *f = riceini_get(cfg, "Fonts", "General");
    if (f && *f)
        t->font_general = g_strdup(f);
    else if (ricedes_font(t->design.des))
        t->font_general = g_strdup(ricedes_font(t->design.des));
    const char *ic = riceini_get(cfg, "Icons", "Theme");
    if (ic && *ic)
        t->icon_theme = g_strdup(ic);
    for (size_t i = 0; i < riceini_entry_count(cfg); ++i) {
        const char *s = riceini_entry_section(cfg, i), *k = riceini_entry_key(cfg, i);
        if (!known(s, k))
            g_ptr_array_add(t->unknown, g_strdup_printf("[%s] %s", s, k));
    }
    (void)k_sections;
    riceini_free(pal);
    riceini_free(cfg);
    g_free(dir);
    g_free(ppath);
    return t;
}

void r4_theme_free(r4_theme *t)
{
    if (!t)
        return;
    g_free(t->font_general);
    g_free(t->icon_theme);
    g_free(t->config_path);
    r4_design_clear(&t->design);
    g_ptr_array_free(t->diag.clamps, TRUE);
    g_ptr_array_free(t->diag.downgrades, TRUE);
    g_ptr_array_free(t->unknown, TRUE);
    g_ptr_array_free(t->problems, TRUE);
    g_ptr_array_free(t->corrections, TRUE);
    g_ptr_array_free(t->elements, TRUE);
    g_free(t);
}
