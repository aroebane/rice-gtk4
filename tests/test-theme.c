/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include "../src/core/css.h"
#include "../src/core/theme.h"

static r4_theme *load_default(void)
{
    g_setenv("RICE_GTK4_CONFIG", "/nonexistent/gtk4.conf", TRUE);
    return r4_theme_load(NULL);
}

static void test_defaults(void)
{
    r4_theme *t = load_default();
    g_assert_nonnull(t);
    g_assert_cmpint(t->shape.border_width, >=, 1);
    r4_theme_free(t);
}

static void test_css(void)
{
    r4_theme *t = load_default();
    GError *err = NULL;
    char *css = r4_css_render(t, &err);
    g_assert_no_error(err);
    g_assert_nonnull(css);
    g_assert_nonnull(strstr(css, "@define-color primary #"));
    g_assert_nonnull(strstr(css, "button, .button {"));
    g_assert_nonnull(strstr(css, "scale slider"));
    g_assert_null(strstr(css, "(null)"));
    /* Every shape token must be filled in. */
    for (const char *p = css; (p = strchr(p, '{')); p++)
        g_assert_false(g_ascii_isupper(p[1]));
    g_assert_false(g_regex_match_simple("--[0-9]", css, 0, 0));
    g_free(css);
    r4_theme_free(t);
}

static void test_index(void)
{
    r4_theme *t = load_default();
    char *idx = r4_index_theme(t);
    g_assert_nonnull(strstr(idx, "Name=rice"));
    g_free(idx);
    r4_theme_free(t);
}

/* A low-contrast palette: the guard raises the roles, or only reports with Enforce=false. */
static r4_theme *load_text(const char *conf)
{
    char *path = g_build_filename(g_get_tmp_dir(), "rice-gtk4-test.conf", NULL);
    g_assert_true(g_file_set_contents(path, conf, -1, NULL));
    g_setenv("RICE_GTK4_CONFIG", path, TRUE);
    r4_theme *t = r4_theme_load(NULL);
    g_remove(path);
    g_free(path);
    return t;
}

static const char *const k_pale =
    "[Colors]\nWindow=#f2f2f2\nBase=#ffffff\nText=#202020\nBorder=#dddddd\nFocus=#cccccc\n";

static void test_guard_raises(void)
{
    r4_theme *t = load_text(k_pale);
    g_assert_cmpuint(t->corrections->len, >, 0);
    g_assert_cmpfloat(r4_contrast(t->border, t->window), >=, 3.0);
    g_assert_cmpfloat(r4_contrast(t->focus, t->base), >=, 3.0);
    r4_theme_free(t);
}

static void test_guard_report_only(void)
{
    char *conf = g_strconcat(k_pale, "[Contrast]\nEnforce=false\n", NULL);
    r4_theme *t = load_text(conf);
    g_free(conf);
    g_assert_cmpuint(t->corrections->len, >, 0);
    g_assert_cmpfloat(r4_contrast(t->border, t->window), <, 3.0);
    r4_theme_free(t);
}

static void test_knob_range(void)
{
    r4_theme *t = load_text("[State]\nHoverLayer=0.9\n[Focus]\nMode=wavy\n");
    g_assert_cmpuint(t->problems->len, ==, 2);
    g_assert_cmpfloat(t->knobs.hover_layer, ==, 0.08);
    r4_theme_free(t);
}

static void test_motion_off(void)
{
    r4_theme *t = load_text("[Motion]\nEnabled=false\n");
    GError *err = NULL;
    char *css = r4_css_render(t, &err);
    g_assert_no_error(err);
    g_assert_nonnull(strstr(css, "transition: none;"));
    g_free(css);
    r4_theme_free(t);
}

static char *render(r4_theme *t)
{
    GError *err = NULL;
    char *css = r4_css_render(t, &err);
    g_assert_no_error(err);
    return css;
}

static void test_status_tint(void)
{
    r4_theme *t = load_default();
    char *css = render(t);
    g_assert_nonnull(strstr(css, "@define-color info_fill"));
    g_assert_nonnull(strstr(css, "@define-color warning_on_fill"));
    g_assert_nonnull(strstr(css, "infobar.warning { background-color: @warning_fill;"));
    g_free(css);
    r4_theme_free(t);
    t = load_text("[State]\nStatusTint=1.5\n");
    g_assert_cmpuint(t->problems->len, ==, 1);
    g_assert_cmpfloat(t->knobs.status_tint, ==, 0.16);
    r4_theme_free(t);
}

/* Every @name the stylesheet uses is defined in it: no dangling colour after the rewrite. */
static void test_names_defined(void)
{
    r4_theme *t = load_default();
    char *css = render(t);
    const char *p = css;
    while ((p = strchr(p, '@')) != NULL) {
        ++p;
        if (!g_ascii_islower(*p))
            continue;
        size_t n = 0;
        while (g_ascii_islower(p[n]) || g_ascii_isdigit(p[n]) || p[n] == '_')
            ++n;
        char *name = g_strndup(p, n);
        char *def = g_strdup_printf("@define-color %s ", name);
        if (strcmp(name, "define") != 0 && strncmp(name, "define-color", 12) != 0 &&
            strncmp(name, "keyframes", 9) != 0 && strncmp(name, "media", 5) != 0)
            if (!strstr(css, def))
                g_error("undefined colour @%s", name);
        g_free(def);
        g_free(name);
        p += n;
    }
    g_free(css);
    r4_theme_free(t);
}

/* These rules belong to an earlier stylesheet; none of them may come back. */
static void test_old_rules_gone(void)
{
    r4_theme *t = load_default();
    char *css = render(t);
    const char *gone[] = {"headerbar .title { font-weight: bold; }",
                          "tab:checked { background-color: alpha(@text, 0.15)",
                          "paned > separator:hover { background-color: @accent",
                          "list row:focus { outline",
                          "infobar.warning { background-color: @warning;",
                          "popover modelbutton:hover, popover modelbutton:selected",
                          NULL};
    for (int i = 0; gone[i]; ++i)
        if (strstr(css, gone[i]))
            g_error("old rule is back: %s", gone[i]);
    g_assert_nonnull(
        strstr(css, "notebook > header tab:checked { background-color: @flat_select_fill;"));
    g_free(css);
    r4_theme_free(t);
}

static void test_emphasis(void)
{
    r4_theme *t = load_text("[Emphasis]\nButton=solid\nInput=flat\n");
    g_assert_cmpuint(t->problems->len, ==, 0);
    char *css = render(t);
    g_assert_nonnull(strstr(css, "[Emphasis]"));
    g_assert_nonnull(strstr(css, "background-color: @primary_hover;"));
    g_free(css);
    r4_theme_free(t);
    t = load_text("[Emphasis]\nButton=loud\nWidget=flat\n");
    g_assert_cmpuint(t->problems->len, ==, 1);
    g_assert_cmpuint(t->unknown->len, ==, 1);
    r4_theme_free(t);
    t = load_default();
    css = render(t);
    g_assert_null(strstr(css, "[Emphasis]"));
    g_free(css);
    r4_theme_free(t);
}

static void test_element_override(void)
{
    r4_theme *t =
        load_text("[Element.button]\nFill=@primary\nText=#ffffff\nRadius=2\nBorder=none\n");
    g_assert_cmpuint(t->problems->len, ==, 0);
    char *css = render(t);
    const char *at = strstr(css, "/* button */");
    g_assert_nonnull(at);
    g_assert_nonnull(strstr(at, "background-color: @primary;"));
    g_assert_nonnull(strstr(at, "border-radius: 2px;"));
    g_free(css);
    r4_theme_free(t);
}

static void test_element_rejects(void)
{
    r4_theme *t = load_text("[Element.no-such-element]\nFill=@primary\n"
                            "[Element.button]\nFill=chartreuse\nRadius=999\nGlow=1\n");
    g_assert_cmpuint(t->problems->len, ==, 3); /* unknown id, bad colour, radius out of range */
    g_assert_cmpuint(t->unknown->len, ==, 1);  /* Glow */
    char *css = render(t);
    g_assert_null(strstr(css, "no-such-element"));
    g_assert_null(strstr(css, "chartreuse"));
    g_free(css);
    r4_theme_free(t);
}

/* A strong tint on a pale palette would leave status text under the floor: the guard lowers it. */
static void test_tint_guard(void)
{
    r4_theme *t = load_text("[Colors]\nWindow=#606060\nText=#a0a0a0\nBase=#ffffff\nAccent=#"
                            "808080\n[State]\nStatusTint=1.0\n");
    g_assert_cmpfloat(t->knobs.status_tint, <, 1.0);
    for (r4_color st[4] = {t->accent, t->positive, t->warning, t->negative}, *c = st; c < st + 4;
         ++c) {
        r4_color fill = r4_mix(t->base, *c, t->knobs.status_tint);
        g_assert_cmpfloat(r4_contrast(r4_readable_on(fill, t->window, t->text), fill), >=, 4.5);
    }
    r4_theme_free(t);
    t = load_text("[Colors]\nWindow=#606060\nText=#a0a0a0\nBase=#ffffff\nAccent=#808080\n[State]"
                  "\nStatusTint=1.0\n[Contrast]\nEnforce=false\n");
    g_assert_cmpfloat(t->knobs.status_tint, ==, 1.0);
    g_assert_cmpuint(t->corrections->len, >, 0);
    r4_theme_free(t);
}

static void test_override_guard(void)
{
    r4_theme *t = load_text("[Element.button]\nFill=#202020\nText=#303030\nBorder=#222222\n");
    char *css = render(t);
    const char *at = strstr(css, "/* button */");
    g_assert_nonnull(at);
    g_assert_null(strstr(at, "color: #303030;"));
    g_assert_null(strstr(at, "border-color: #222222;"));
    g_assert_cmpuint(t->corrections->len, >=, 2);
    g_free(css);
    r4_theme_free(t);
    t = load_text("[Element.button]\nFill=#202020\nText=#303030\n[Contrast]\nEnforce=false\n");
    css = render(t);
    g_assert_nonnull(strstr(css, "color: #303030;"));
    g_free(css);
    r4_theme_free(t);
}

/* The producer's palette file carries the shell's own state colours and amounts: they replace the
 * engine's derivation, and gtk4.conf still wins over them. */
static r4_theme *load_pair(const char *conf, const char *palette)
{
    char *ppath = g_build_filename(g_get_tmp_dir(), "rice-gtk4-test-palette.ini", NULL);
    char *full = g_strdup_printf("[Source]\nPath=%s\n%s", ppath, conf);
    g_assert_true(g_file_set_contents(ppath, palette, -1, NULL));
    r4_theme *t = load_text(full);
    g_remove(ppath);
    g_free(full);
    g_free(ppath);
    return t;
}

static const char *const k_states =
    "[Palette]\nbackground=#101010\nsurface=#202020\ntext=#e0e0e0\naccent=#80c0ff\n"
    "[State]\nHoverLayer=0.2\n"
    "[Control]\nprimary=#80c0ff\nprimaryHover=#123456\n"
    "primaryPressed=#654321\nhoverSurface=#345678\ndisabledText=#555555\n";

static void test_shell_states(void)
{
    r4_theme *t = load_pair("", k_states);
    g_assert_cmpfloat(t->knobs.hover_layer, ==, 0.2);
    char *css = render(t);
    g_assert_nonnull(strstr(css, "@define-color primary_hover #123456"));
    g_assert_nonnull(strstr(css, "@define-color base_hover #345678"));
    g_assert_nonnull(strstr(css, "@define-color disabled_text #555555"));
    g_free(css);
    r4_theme_free(t);
}

static void test_shell_states_yield(void)
{
    /* gtk4.conf wins over the palette's amount, and a config colour makes the producer's states for
     * that colour stale: the engine derives them again. */
    r4_theme *t = load_pair("[State]\nHoverLayer=0.1\n[Colors]\nPrimary=#ff0000\n", k_states);
    g_assert_cmpfloat(t->knobs.hover_layer, ==, 0.1);
    char *css = render(t);
    g_assert_null(strstr(css, "@define-color primary_hover #123456"));
    g_assert_nonnull(strstr(css, "@define-color base_hover #345678"));
    g_free(css);
    r4_theme_free(t);
}

static void test_pointer_hover_off(void)
{
    r4_theme *t = load_pair("[State]\nPointerHover=false\n", k_states);
    g_assert_cmpuint(t->problems->len, ==, 0);
    char *css = render(t);
    g_assert_nonnull(strstr(css, "@define-color primary_hover #80c0ff"));
    g_assert_nonnull(strstr(css, "@define-color base_hover #202020"));
    g_free(css);
    r4_theme_free(t);
}

/* What each emphasis paints per state arrives as role names; the engine resolves them against its
 * own colours. */
static const char *const k_paint = "[Control.outline]\nrestBorder=none\nhoverBorder="
                                   "outline\nselectFill=container\nselectBorder=outline\n"
                                   "disabledBorder=disabledBorder\n"
                                   "[Control.flat]\nselectFill=primaryPressed\nselectBorder=none\n";

static void test_paint_table(void)
{
    r4_theme *t = load_pair("", g_strconcat(k_states, k_paint, NULL));
    g_assert_cmpuint(t->problems->len, ==, 0);
    char *css = render(t);
    /* "none" is a transparent boundary; "outline" is the engine's own boundary colour. */
    g_assert_nonnull(strstr(css, "@define-color outline_rest_border rgba(0, 0, 0, 0.000);"));
    g_assert_nonnull(strstr(css, "@define-color flat_select_border rgba(0, 0, 0, 0.000);"));
    g_assert_nonnull(strstr(css, "@define-color outline_hover_border #"));
    g_assert_nonnull(strstr(css, "@define-color flat_select_fill #654321;"));
    /* A slot the producer did not name keeps the engine's own look. */
    g_assert_nonnull(strstr(css, "@define-color solid_pressed_fill #654321;"));
    g_free(css);
    r4_theme_free(t);
}

static void test_paint_follows_overrides(void)
{
    /* A role name resolves to the engine's colour, so a [Colors] override and the guard carry
     * through. */
    r4_theme *t = load_pair("[Colors]\nPrimary=#ff0000\n", g_strconcat(k_states, k_paint, NULL));
    char *css = render(t);
    g_assert_null(strstr(css, "@define-color flat_select_fill #654321;"));
    g_assert_nonnull(strstr(css, "@define-color flat_select_fill #"));
    g_free(css);
    r4_theme_free(t);
}

static void test_paint_unknown_name(void)
{
    r4_theme *t =
        load_pair("", g_strconcat(k_states, "[Control.flat]\nselectFill=sparkle\n", NULL));
    g_assert_cmpuint(t->problems->len, ==, 1);
    r4_theme_free(t);
}

static void test_focus_visible(void)
{
    r4_theme *t = load_pair("", g_strconcat(k_states, "[Focus]\nVisible=always\n", NULL));
    g_assert_cmpuint(t->problems->len, ==, 0);
    g_assert_true(t->knobs.focus_always);
    char *css = render(t);
    g_assert_nonnull(strstr(css, "\nbutton:focus, entry:focus-visible"));
    g_assert_null(strstr(css, "\nbutton:focus-visible, entry:focus-visible"));
    g_free(css);
    r4_theme_free(t);
    t = load_pair("", k_states);
    css = render(t);
    g_assert_nonnull(strstr(css, "\nbutton:focus-visible, entry:focus-visible"));
    g_free(css);
    r4_theme_free(t);
    t = load_pair("", g_strconcat(k_states, "[Focus]\nVisible=sometimes\n", NULL));
    g_assert_cmpuint(t->problems->len, ==, 1);
    r4_theme_free(t);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/theme/defaults", test_defaults);
    g_test_add_func("/css/render", test_css);
    g_test_add_func("/css/index", test_index);
    g_test_add_func("/guard/raises", test_guard_raises);
    g_test_add_func("/guard/report-only", test_guard_report_only);
    g_test_add_func("/knobs/range", test_knob_range);
    g_test_add_func("/motion/off", test_motion_off);
    g_test_add_func("/state/status-tint", test_status_tint);
    g_test_add_func("/css/names-defined", test_names_defined);
    g_test_add_func("/css/old-rules-gone", test_old_rules_gone);
    g_test_add_func("/override/emphasis", test_emphasis);
    g_test_add_func("/override/element", test_element_override);
    g_test_add_func("/override/rejects", test_element_rejects);
    g_test_add_func("/guard/status-tint", test_tint_guard);
    g_test_add_func("/guard/overrides", test_override_guard);
    g_test_add_func("/state/shell", test_shell_states);
    g_test_add_func("/state/shell-yields", test_shell_states_yield);
    g_test_add_func("/state/pointer-hover-off", test_pointer_hover_off);
    g_test_add_func("/paint/table", test_paint_table);
    g_test_add_func("/paint/follows-overrides", test_paint_follows_overrides);
    g_test_add_func("/paint/unknown-name", test_paint_unknown_name);
    g_test_add_func("/focus/visible", test_focus_visible);
    return g_test_run();
}
