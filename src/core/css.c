/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css.h"

#include <string.h>

#include "elements.h"

extern const unsigned char r4_css_template[];
extern const size_t r4_css_template_len;

static void define(GString *o, const char *name, r4_color c)
{
    char h[8];
    r4_hex(c, h);
    const uint32_t argb = ricecol_to_argb8(c);
    const unsigned a = (argb >> 24U) & 255U;
    if (a == 255U) {
        g_string_append_printf(o, "@define-color %s %s;\n", name, h);
    } else {
        const unsigned v[3] = {(argb >> 16U) & 255U, (argb >> 8U) & 255U, argb & 255U};
        char alpha[G_ASCII_DTOSTR_BUF_SIZE];
        (void)g_ascii_formatd(alpha, sizeof alpha, "%.3f", a / 255.0);
        g_string_append_printf(
            o, "@define-color %s rgba(%u, %u, %u, %s);\n", name, v[0], v[1], v[2], alpha);
    }
}

/* A [Control] role from the producer, else the engine's own derivation. */
static gboolean has_ctl(const r4_theme *t, r4_ctl i)
{
    return ((t->ctl_set >> (unsigned)i) & 1U) != 0U;
}

static r4_color pick(const r4_theme *t, r4_ctl i, r4_color derived)
{
    return has_ctl(t, i) ? t->ctl[i] : derived;
}

/* A hover colour; the resting one when the pointer is not to be heard (PointerHover=false). */
static r4_color hov(const r4_theme *t, r4_color hovered, r4_color rest)
{
    return t->knobs.pointer_hover ? hovered : rest;
}

/* The colour the engine gives a role name of the producer's behaviour profile. The producer chooses
 * the role (what each emphasis paints in each state); the colour stays the engine's own, so its
 * [Colors] overrides, the contrast guard and the amounts apply. */
static r4_color role_color(const r4_theme *t, r4_role r, r4_color base)
{
    switch (r) {
    case R4_ROLE_NONE:
        return ricecol_from_rgba16(0, 0, 0, 0);
    case R4_ROLE_SURFACE:
        return base;
    case R4_ROLE_HOVER_SURFACE:
    case R4_ROLE_FOCUS_SURFACE:
        return pick(t, R4_CTL_HOVER_SURFACE, r4_hover(t, base));
    case R4_ROLE_PRESSED_SURFACE:
        return pick(t, R4_CTL_PRESSED_SURFACE, r4_pressed(t, base));
    case R4_ROLE_CONTAINER:
    case R4_ROLE_CURRENT_CONTAINER:
        return t->container;
    case R4_ROLE_CONTAINER_HOVER:
        return pick(t, R4_CTL_CONTAINER_HOVER, r4_hover(t, t->container));
    case R4_ROLE_CONTAINER_PRESSED:
        return pick(t, R4_CTL_CONTAINER_PRESSED, r4_pressed(t, t->container));
    case R4_ROLE_DISABLED_SURFACE:
        return r4_disabled_fill(t, t->base);
    case R4_ROLE_PRIMARY:
    case R4_ROLE_CURRENT:
        return t->primary;
    case R4_ROLE_PRIMARY_HOVER:
    case R4_ROLE_PRIMARY_FOCUS:
        return pick(t, R4_CTL_PRIMARY_HOVER, r4_hover(t, t->primary));
    case R4_ROLE_PRIMARY_PRESSED:
        return pick(t, R4_CTL_PRIMARY_PRESSED, r4_pressed(t, t->primary));
    case R4_ROLE_TEXT:
        return t->text;
    case R4_ROLE_MUTED:
        return t->muted;
    case R4_ROLE_CONTAINER_TEXT:
    case R4_ROLE_CURRENT_TEXT:
        return t->container_text;
    case R4_ROLE_DISABLED_TEXT:
        return pick(t, R4_CTL_DISABLED_TEXT, r4_disabled_text(t, t->text));
    case R4_ROLE_PRIMARY_TEXT:
        return t->primary_text;
    case R4_ROLE_OUTLINE:
        return t->border;
    case R4_ROLE_FOCUS:
        return t->focus;
    case R4_ROLE_HOVER:
        return r4_mix(t->border, t->text, 2 * t->knobs.hover_layer);
    case R4_ROLE_DISABLED_BORDER:
        /* The enabled boundary dimmed like disabled text, so a disabled control keeps an outline
         * (one equal to its fill would vanish). */
        return r4_disabled_text(t, t->border);
    case R4_ROLE_UNSET:
    case R4_ROLE_COUNT:
        break;
    }
    return t->text;
}

/* What each emphasis paints per state, as @{outline,flat,solid}_<slot>. A slot the producer did not
 * name keeps the engine's own look. The paint states of a family are the same set for every
 * widget of that emphasis; the template picks the family each widget belongs to. */
static void paint_colors(GString *o, const r4_theme *t)
{
    static const char *const emph[R4_PE_COUNT] = {"outline", "flat", "solid"};
    static const char *const slot[R4_PS_COUNT] = {"rest_border",
                                                  "hover_border",
                                                  "pressed_fill",
                                                  "pressed_border",
                                                  "select_fill",
                                                  "select_text",
                                                  "select_border",
                                                  "disabled_fill",
                                                  "disabled_border"};
    const r4_color none = ricecol_from_rgba16(0, 0, 0, 0);
    const r4_color sel_border = t->knobs.selection_boundary ? t->primary : t->border;
    const r4_color hover_border = r4_mix(t->border, t->text, 2 * t->knobs.hover_layer);
    for (int e = 0; e < R4_PE_COUNT; ++e) {
        const gboolean solid = e == R4_PE_SOLID, flat = e == R4_PE_FLAT;
        /* What "surface" means: the fill the widgets of this emphasis rest on. */
        const r4_color rest_fill = solid ? t->primary : flat ? t->base : t->button;
        /* The engine's own look, by emphasis (what the template did before the table existed). */
        const r4_color own[R4_PS_COUNT] = {
            solid  ? t->primary
            : flat ? none
                   : t->border,
            solid  ? t->primary
            : flat ? none
                   : hover_border,
            solid  ? pick(t, R4_CTL_PRIMARY_PRESSED, r4_pressed(t, t->primary))
            : flat ? pick(t, R4_CTL_PRESSED_SURFACE, r4_pressed(t, t->base))
                   : r4_pressed(t, t->button),
            solid  ? t->primary
            : flat ? none
                   : hover_border,
            solid  ? t->primary
            : flat ? pick(t, R4_CTL_SELECTION, t->container)
                   : t->container,
            solid  ? t->primary_text
            : flat ? pick(t, R4_CTL_SELECTION_TEXT, t->container_text)
                   : t->container_text,
            solid ? t->primary : sel_border,
            r4_disabled_fill(t, rest_fill),
            r4_disabled_text(t, t->border)};
        r4_color c[R4_PS_COUNT];
        for (int s = 0; s < R4_PS_COUNT; ++s)
            c[s] =
                t->paint[e][s] != R4_ROLE_UNSET ? role_color(t, t->paint[e][s], rest_fill) : own[s];
        /* The pointer is not to be heard (PointerHover=false): a hover border is the resting one.
         */
        c[R4_PS_HOVER_BORDER] = hov(t, c[R4_PS_HOVER_BORDER], c[R4_PS_REST_BORDER]);
        char nm[48];
        for (int s = 0; s < R4_PS_COUNT; ++s) {
            (void)g_snprintf(nm, sizeof nm, "%s_%s", emph[e], slot[s]);
            define(o, nm, c[s]);
        }
        /* An input rests on the base, not the raised button fill. */
        if (e == R4_PE_OUTLINE) {
            define(o,
                   "outline_disabled_base",
                   t->paint[e][R4_PS_DISABLED_FILL] != R4_ROLE_UNSET
                       ? role_color(t, t->paint[e][R4_PS_DISABLED_FILL], t->base)
                       : r4_disabled_fill(t, t->base));
        }
        /* A selected control under the pointer: the selected fill lifted (a solid one is not). */
        (void)g_snprintf(nm, sizeof nm, "%s_select_hover", emph[e]);
        define(o,
               nm,
               solid ? c[R4_PS_SELECT_FILL]
                     : hov(t, r4_hover(t, c[R4_PS_SELECT_FILL]), c[R4_PS_SELECT_FILL]));
    }
}

static void colors(GString *o, const r4_theme *t)
{
    r4_color neg_text = r4_readable_on(t->negative, t->window, t->text);
    r4_color warn_text = r4_readable_on(t->warning, t->window, t->text);
    define(o, "window", t->window);
    define(o, "text", t->text);
    define(o, "base", t->base);
    define(o, "base_hover", hov(t, pick(t, R4_CTL_HOVER_SURFACE, r4_hover(t, t->base)), t->base));
    define(o, "button", t->button);
    define(o, "button_hover", hov(t, r4_hover(t, t->button), t->button));
    define(o, "accent", t->accent);
    define(o, "visited", pick(t, R4_CTL_VISITED, r4_mix(t->accent, t->muted, 0.5)));
    define(o, "border", t->border);
    define(o, "focus", t->focus);
    define(o, "muted", t->muted);
    define(o, "tab_text", pick(t, R4_CTL_TAB_TEXT, t->text));
    define(o, "container", t->container);
    define(o, "container_text", t->container_text);
    define(o, "primary", t->primary);
    define(o,
           "primary_hover",
           hov(t, pick(t, R4_CTL_PRIMARY_HOVER, r4_hover(t, t->primary)), t->primary));
    define(o, "primary_pressed", pick(t, R4_CTL_PRIMARY_PRESSED, r4_pressed(t, t->primary)));
    define(o, "primary_text", t->primary_text);
    define(o, "positive", t->positive);
    define(o, "warning", t->warning);
    define(o, "warning_text", warn_text);
    define(o, "negative", t->negative);
    define(o, "negative_hover", hov(t, r4_hover(t, t->negative), t->negative));
    define(o, "negative_text", neg_text);
    define(o, "disabled_text", pick(t, R4_CTL_DISABLED_TEXT, r4_disabled_text(t, t->text)));
    define(o, "disabled_fill", r4_disabled_fill(t, t->button));
    define(o, "disabled_base", r4_disabled_fill(t, t->base));
    /* The enabled boundary dimmed like disabled text, so a disabled control keeps an outline (one
     * equal to its fill would vanish). The check, radio and switch parts use it as it is. */
    define(o, "disabled_border", r4_disabled_text(t, t->border));
    define(o, "track", pick(t, R4_CTL_TRACK, r4_mix(t->window, t->text, 0.14)));
    define(o, "thumb", pick(t, R4_CTL_THUMB, r4_mix(t->window, t->text, 0.35)));
    define(o,
           "thumb_hover",
           hov(t,
               pick(t, R4_CTL_THUMB_HOVER, r4_mix(t->window, t->text, 0.5)),
               pick(t, R4_CTL_THUMB, r4_mix(t->window, t->text, 0.35))));
    define(o,
           "track_hover",
           hov(t,
               pick(t, R4_CTL_TRACK_HOVER, pick(t, R4_CTL_TRACK, r4_mix(t->window, t->text, 0.14))),
               pick(t, R4_CTL_TRACK, r4_mix(t->window, t->text, 0.14))));
    define(o, "surface_variant", t->surface_variant);
    define(o, "scrim", t->scrim);
    define(o, "selection", pick(t, R4_CTL_SELECTION, t->container));
    define(o,
           "selection_text",
           pick(t,
                R4_CTL_SELECTION_TEXT,
                has_ctl(t, R4_CTL_SELECTION)
                    ? r4_readable_on(pick(t, R4_CTL_SELECTION, t->container), t->window, t->text)
                    : t->container_text));
    define(o,
           "disabled_strong",
           pick(t,
                R4_CTL_DISABLED_STRONG,
                r4_mix(t->window, t->text, 2 * t->knobs.disabled_container)));
    define(o, "border_subtle", pick(t, R4_CTL_BORDER_SUBTLE, t->border));
    define(o, "border_rule", pick(t, R4_CTL_BORDER_RULE, t->border));
    define(o,
           "border_resting_link",
           pick(t, R4_CTL_BORDER_RESTING_LINK, ricecol_from_rgba16(0, 0, 0, 0)));
    /* Raised surface (palette `elevated` is the button fill), the info status, tonal status fills.
     */
    define(o, "elevated", t->button);
    define(o, "elevated_hover", hov(t, r4_hover(t, t->button), t->button));
    define(o, "window_hover", hov(t, r4_hover(t, t->window), t->window));
    define(o, "info", t->accent);
    define(o, "info_text", r4_readable_on(t->accent, t->window, t->text));
    define(o, "positive_text", r4_readable_on(t->positive, t->window, t->text));
    {
        const struct {
            const char *name;
            r4_color c;
        } st[] = {{"info", t->accent},
                  {"positive", t->positive},
                  {"warning", t->warning},
                  {"negative", t->negative}};
        for (guint i = 0; i < G_N_ELEMENTS(st); i++) {
            char nm[32];
            r4_color fill = r4_mix(t->base, st[i].c, t->knobs.status_tint);
            (void)g_snprintf(nm, sizeof nm, "%s_fill", st[i].name);
            define(o, nm, fill);
            (void)g_snprintf(nm, sizeof nm, "%s_on_fill", st[i].name);
            define(o, nm, r4_readable_on(fill, t->window, t->text));
        }
    }
    define(o, "thumb_active", pick(t, R4_CTL_THUMB_ACTIVE, r4_mix(t->window, t->text, 0.65)));
    paint_colors(o, t);
}

/* Value of a {TOKEN}; NULL when unknown. Returned string is owned by the caller. */
/* {R_<NAME>}: the border-radius value (with its px) of one component family. TRACK is the
 * slider's groove, a thinner box than its knob. NULL for a name that is no family. */
static char *radius_token(const r4_theme *t, const char *name)
{
    const r4_shape *s = &t->shape;
    double w, h;
    r4_geometry g;
    int f = -1;
    if (!strcmp(name, "TRACK")) {
        f = R4_FAM_SLIDER;
        w = s->indicator_size;
        h = 2 * MAX(s->indicator_size / 4, 1);
    } else {
        for (int i = 0; i < R4_FAM_COUNT && f < 0; ++i) {
            char *up = g_ascii_strup(r4_family_name((r4_family)i), -1);
            if (!strcmp(up, name))
                f = i;
            g_free(up);
        }
        if (f < 0)
            return NULL;
        r4_family_box(s, (r4_family)f, &w, &h);
    }
    r4_design_geometry(&t->design, s, (r4_family)f, w, h, &g, (r4_diag *)&t->diag);
    return r4_geometry_css(&g);
}

/* Value of a {TOKEN}; NULL when unknown. Returned string is owned by the caller. */
static char *token(const r4_theme *t, const char *name)
{
    const r4_shape *s = &t->shape;
    int inner = MAX(s->control_height - 2 * s->border_width, 1);
    int slider = MAX(s->indicator_size * 3 / 4, 4), track = MAX(s->indicator_size / 4, 2);
    int scroll_vis = MAX(s->scrollbar_width / 2, 2);
    int bar_pad = s->spacing / 2; /* bar = control + one spacing (control height + 2 * bar_pad) */
    int sw_h = s->indicator_size + 2;
    struct {
        const char *k;
        int v;
    } n[] = {
        {"BORDER", s->border_width},
        {"HEIGHT_INNER", inner},
        {"HEIGHT", s->control_height},
        {"SEAM_SIDE", inner + s->border_width},
        {"BAR_PAD", bar_pad},
        {"BAR_H", s->control_height + 2 * bar_pad},
        {"SPIN_BTN", MAX(inner - 2 * (s->spacing / 2), 1)},
        {"SWITCH_BAR", MAX((s->control_height + 2 * bar_pad - sw_h - 2 * s->border_width) / 2, 0)},
        {"BAR_TAB_PAD", s->padding + s->border_width},
        {"MENU_H", MAX(inner - 4 * s->border_width, 1)},
        {"MENU_W", 2 * s->control_height},
        {"MENU_PAD", s->padding * 3 / 2},
        {"ACCEL", 5 * s->spacing},
        {"MARK_OUT", 2 * s->spacing},
        {"LEVEL_W", s->spacing * 4 / 3},
        {"PADDING", s->padding},
        {"PADDING_WIDE", s->padding + 2},
        {"PADDING_TAB", s->padding * 2},
        {"PAGE_PAD", 3 * s->spacing},
        {"RING", 2 * s->border_width},
        {"RING_ACTIVE", 4 * s->border_width},
        {"HALO_SMALL", s->indicator_size * 3 / 8},
        {"HALO", s->indicator_size / 2},
        {"ITEM_V", MAX((inner - 16) / 2, 0)},
        {"SPACING", s->spacing},
        {"RADIO_PAD", MAX(s->indicator_size / 4, 1)},
        {"RADIO_DOT", MAX(s->indicator_size - 2 * MAX(s->indicator_size / 4, 1), 1)},
        {"SPACING_HALF", s->spacing / 2},
        {"FOCUS", s->focus_width},
        {"FOCUS_IN", -s->focus_offset},
        {"FOCUS_OUT", s->focus_offset},
        {"INDICATOR", s->indicator_size},
        {"SWITCH_H", sw_h},
        {"SWITCH_W", 2 * sw_h},
        {"SLIDER", slider},
        {"SLIDER_MARGIN", -(slider - track) / 2},
        {"TRACK", track},
        {"SCROLL_VIS", scroll_vis},
        {"SCROLL_GAP", s->scrollbar_width - scroll_vis},
        {"SCROLL", s->scrollbar_width},
        {"SCROLL_HALF", s->scrollbar_width / 2},
        {"SCROLL_LEN", 3 * s->scrollbar_width},
        {"SCALE_PAD", slider / 2 + s->spacing},
    };
    if (name[0] == 'R' && name[1] == '_')
        return radius_token(t, name + 2);
    /* A focus ring sits on the border footprint (inset), outside it (outer), or is the border
     * itself (border). The mode is [Focus] Mode. */
    if (!strcmp(name, "RING_OFFSET")) {
        int off = t->knobs.focus_mode == R4_FOCUS_OUTER    ? s->focus_offset
                  : t->knobs.focus_mode == R4_FOCUS_BORDER ? -s->border_width
                                                           : -s->focus_width;
        return g_strdup_printf("%d", off);
    }
    /* The pseudo-class that shows the ring: keyboard focus only, or any focus ([Focus] Visible).
     * Text inputs show theirs on any focus in GTK 4 already, so they keep :focus-visible. */
    if (!strcmp(name, "FV"))
        return g_strdup(t->knobs.focus_always ? "focus" : "focus-visible");
    /* Colour-only transitions, none when motion is off or the duration is zero (reduced motion). */
    if (!strcmp(name, "TRANSITION")) {
        if (!t->knobs.motion || t->knobs.motion_ms <= 0)
            return g_strdup("none");
        return g_strdup_printf("background-color %dms cubic-bezier(0.215, 0.61, 0.355, 1), "
                               "border-color %dms cubic-bezier(0.215, 0.61, 0.355, 1), "
                               "color %dms cubic-bezier(0.215, 0.61, 0.355, 1)",
                               t->knobs.motion_ms,
                               t->knobs.motion_ms,
                               t->knobs.motion_ms);
    }
    for (guint i = 0; i < G_N_ELEMENTS(n); i++)
        if (!strcmp(n[i].k, name))
            return g_strdup_printf("%d", n[i].v);
    return NULL;
}

static char *load_template(void)
{
    return g_strndup((const char *)r4_css_template, r4_css_template_len);
}

/* ---- [Emphasis] and [Element.<id>]: a layer after the template ------------------------ */

typedef struct {
    const char *key;
    const char *const *sel;
    const char *fill; /* the role an outlined family rests on */
} emph_family;

static const char *const s_button[] = {
    "button:not(.flat):not(.link):not(.suggested-action):not(.destructive-action)", NULL};
static const char *const s_input[] = {"entry", "spinbutton", NULL};
static const char *const s_tab[] = {"notebook > header tab", NULL};
static const char *const s_row[] = {"list row", "listview > row", "gridview > child", NULL};
static const char *const s_menu[] = {
    "menu menuitem", "popover modelbutton", "menubar > menuitem", NULL};
static const emph_family k_emph[R4_EF_COUNT] = {
    {"Button", s_button, "@button"},
    {"Input", s_input, "@base"},
    {"Tab", s_tab, "@window"},
    {"Row", s_row, "@base"},
    {"MenuItem", s_menu, "@elevated"},
};

/* Each selector with the states it must not touch, so only the resting look changes. */
static void resting(GString *o, const char *const *sel, const char *extra)
{
    for (int i = 0; sel[i]; ++i)
        g_string_append_printf(
            o,
            "%s%s%s",
            i ? ",\n" : "",
            sel[i],
            ":not(:hover):not(:active):not(:checked):not(:selected):not(:disabled)");
    g_string_append_printf(o, " { %s }\n", extra);
}

static void hovered(GString *o, const char *const *sel, const char *extra)
{
    for (int i = 0; sel[i]; ++i)
        g_string_append_printf(o, "%s%s:hover:not(:active):not(:disabled)", i ? ",\n" : "", sel[i]);
    g_string_append_printf(o, " { %s }\n", extra);
}

static char *emphasis_decl(const r4_theme *t, r4_emphasis e, const char *fill)
{
    switch (e) {
    case R4_EMPH_OUTLINE:
        return g_strdup_printf(
            "background-color: %s; border: %dpx solid @border;", fill, t->shape.border_width);
    case R4_EMPH_FLAT:
        return g_strdup("background-color: transparent; border-color: transparent;");
    case R4_EMPH_SOLID:
        return g_strdup_printf("background-color: @primary; color: @primary_text; "
                               "border: %dpx solid @primary;",
                               t->shape.border_width);
    default:
        return NULL;
    }
}

static void overrides(GString *o, const r4_theme *t)
{
    gboolean head = FALSE;
    for (int f = 0; f < R4_EF_COUNT; ++f) {
        char *d = emphasis_decl(t, t->emphasis[f], k_emph[f].fill);
        if (!d)
            continue;
        if (!head++)
            g_string_append(
                o, "\n/* [Emphasis] ------------------------------------------------------ */\n");
        resting(o, k_emph[f].sel, d);
        if (t->emphasis[f] == R4_EMPH_SOLID)
            hovered(o, k_emph[f].sel, "background-color: @primary_hover;");
        else if (t->emphasis[f] == R4_EMPH_FLAT)
            hovered(o, k_emph[f].sel, "background-color: @elevated_hover;");
        g_free(d);
    }
    for (guint i = 0; i < t->elements->len; ++i) {
        const r4_element_over *e = t->elements->pdata[i];
        const r4_element_def *def = r4_element_find(e->id);
        GString *sel = g_string_new(NULL), *d = g_string_new(NULL);
        for (int h = 0; def && def->hooks[h]; ++h) {
            if (strchr(def->hooks[h], ':')) /* state hooks keep the template's look */
                continue;
            g_string_append_printf(sel, "%s%s", sel->len ? ",\n" : "", def->hooks[h]);
        }
        if (!sel->len) {
            g_string_free(sel, TRUE);
            g_string_free(d, TRUE);
            continue;
        }
        char *em = emphasis_decl(t, e->emphasis, "@button");
        if (em)
            g_string_append_printf(d, "%s ", em);
        g_free(em);
        if (e->fill)
            g_string_append_printf(d, "background-color: %s; ", e->fill);
        if (e->text)
            g_string_append_printf(d, "color: %s; ", e->text);
        if (e->border)
            g_string_append_printf(d, "border-style: solid; border-color: %s; ", e->border);
        if (e->border_width >= 0)
            g_string_append_printf(d, "border-style: solid; border-width: %dpx; ", e->border_width);
        if (e->radius >= 0)
            g_string_append_printf(d, "border-radius: %dpx; ", e->radius);
        if (e->pad_x >= 0 || e->pad_y >= 0)
            g_string_append_printf(d,
                                   "padding: %dpx %dpx; ",
                                   e->pad_y >= 0 ? e->pad_y : 0,
                                   e->pad_x >= 0 ? e->pad_x : t->shape.padding);
        if (e->min_height >= 0)
            g_string_append_printf(d, "min-height: %dpx; ", e->min_height);
        if (d->len) {
            if (!head++)
                g_string_append(
                    o, "\n/* [Element.<id>] ---------------------------------------------- */\n");
            g_string_append_printf(o, "/* %s */\n%s { %s}\n", e->id, sel->str, d->str);
        }
        g_string_free(sel, TRUE);
        g_string_free(d, TRUE);
    }
}

char *r4_css_render(const r4_theme *t, GError **err)
{
    char *tpl = load_template();
    if (!tpl)
        return NULL;
    GString *o = g_string_new("/* Generated by rice-gtk4-css. Do not edit. */\n");
    colors(o, t);
    g_string_append_c(o, '\n');
    for (const char *p = tpl; *p;) {
        const char *end = (*p == '{' && p[1] >= 'A' && p[1] <= 'Z') ? strchr(p, '}') : NULL;
        if (end) {
            char *name = g_strndup(p + 1, end - p - 1);
            gboolean ident = strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_") == strlen(name);
            char *v = ident ? token(t, name) : NULL;
            if (ident && !v) {
                g_set_error(
                    err, G_FILE_ERROR, G_FILE_ERROR_FAILED, "unknown token {%s} in template", name);
                g_free(name);
                g_string_free(o, TRUE);
                g_free(tpl);
                return NULL;
            }
            if (v) {
                g_string_append(o, v);
                g_free(v);
                g_free(name);
                p = end + 1;
                continue;
            }
            g_free(name);
        }
        g_string_append_c(o, *p++);
    }
    g_free(tpl);
    overrides(o, t);
    return g_string_free(o, FALSE);
}

char *r4_index_theme(const r4_theme *t)
{
    (void)t;
    return g_strdup(
        "[Desktop Entry]\nType=X-GNOME-Metatheme\nName=rice\nComment=Generated by rice-gtk4\n"
        "\n[X-GNOME-Metatheme]\nGtkTheme=rice\n");
}
