/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include "../src/core/css.h"
#include "../src/core/theme.h"

static char *tmp;

static void put(const char *name, const char *text)
{
    char *p = g_build_filename(tmp, name, NULL);
    g_assert_true(g_file_set_contents(p, text, -1, NULL));
    g_free(p);
}

/* A fresh config dir holding gtk4.conf and designs/, with the design env cleared. */
static r4_theme *load(const char *conf)
{
    g_unsetenv("RICE_GTK4_DESIGN");
    g_unsetenv("RICE_GTK4_DESIGN_PATH");
    g_setenv("RICE_GTK4_DESIGN_DIR", tmp, TRUE);
    put("gtk4.conf", conf);
    char *p = g_build_filename(tmp, "gtk4.conf", NULL);
    r4_theme *t = r4_theme_load(p);
    g_free(p);
    return t;
}

static char *css_of(const r4_theme *t)
{
    GError *err = NULL;
    char *c = r4_css_render(t, &err);
    g_assert_no_error(err);
    return c;
}

static gboolean has(const GPtrArray *a, const char *needle)
{
    for (guint i = 0; i < a->len; ++i)
        if (strstr(a->pdata[i], needle))
            return TRUE;
    return FALSE;
}

static void test_chain(void)
{
    put("base.ini", "[Design]\nSchema=1\nFamily=sharp\n[Metrics]\nSpacing=3\n");
    put("kid.ini",
        "[Design]\nName=kid\nSchema=1\nInherits=base\n[Radius]\nsm=5\n[Shape.button]\nRadius="
        "7px\n");
    r4_theme *t = load("[Design]\nName=kid\n");
    g_assert_cmpint(t->problems->len, ==, 0);
    g_assert_cmpstr(ricedes_name(t->design.des), ==, "kid");
    g_assert_cmpint(ricedes_chain_count(t->design.des), ==, 2);
    g_assert_cmpint(t->shape.spacing, ==, 3); /* from the parent */
    char *c = css_of(t);
    g_assert_nonnull(strstr(c, "border-radius: 7px")); /* the explicit button radius */
    g_free(c);
    r4_theme_free(t);
}

static void test_config_over_design(void)
{
    put("d.ini", "[Design]\nSchema=1\n[Radius]\nsm=9\n[Metrics]\nSpacing=3\n");
    r4_theme *t = load("[Design]\nName=d\n[Shape]\nRadius=2\nSpacing=8\n");
    g_assert_cmpint(t->shape.spacing, ==, 8); /* the config's own key wins */
    r4_geometry g;
    r4_design_geometry(&t->design, &t->shape, R4_FAM_BUTTON, 0, 0, &g, NULL);
    g_assert_cmpfloat(g.radius[0], ==, 2); /* legacy Radius beats the design's slot */
    r4_theme_free(t);
}

static void test_integer_only_toolkit(void)
{
    r4_theme *t = load("[Shape]\nRadius=3.5\n");
    r4_geometry g;
    r4_design_geometry(&t->design, &t->shape, R4_FAM_BUTTON, 0, 0, &g, &t->diag);
    g_assert_cmpfloat(g.radius[0], ==, 3.5); /* GTK 4 takes a fractional radius */
    g_assert_cmpint(t->diag.downgrades->len, ==, 0);
    t->design.caps.fractional_radius = FALSE; /* a toolkit that cannot */
    r4_design_geometry(&t->design, &t->shape, R4_FAM_BUTTON, 0, 0, &g, &t->diag);
    g_assert_cmpfloat(g.radius[0], ==, 4);
    g_assert_true(has(t->diag.downgrades, "button: radius 3.5 px drawn as 4 px"));
    r4_theme_free(t);
}

static void test_cycle_and_missing(void)
{
    put("a.ini", "[Design]\nInherits=b\n");
    put("b.ini", "[Design]\nInherits=a\n");
    r4_theme *t = load("[Design]\nName=a\n");
    g_assert_true(has(t->problems, "loops back"));
    g_assert_null(ricedes_name(t->design.des)); /* a broken chain applies nothing */
    r4_theme_free(t);
    t = load("[Design]\nName=nope\n");
    g_assert_true(has(t->problems, "not found"));
    g_assert_null(ricedes_name(t->design.des));
    r4_theme_free(t);
    t = load("[Design]\nName=../etc/passwd\n");
    g_assert_true(has(t->problems, "not found")); /* a name is never a path */
    r4_theme_free(t);
}

static void test_clamps_and_unknown(void)
{
    put("c.ini",
        "[Design]\nSchema=1\n[Metrics]\nControlHeight=5\n[Shape.button]\nRadius=999\n"
        "[Radius]\nbogus=1\n[Shape.nothing]\nRadius=1\n");
    r4_theme *t = load("[Design]\nName=c\n");
    g_assert_cmpint(t->shape.control_height, ==, 12);
    g_assert_true(has(t->diag.clamps, "ControlHeight"));
    g_assert_true(has(t->diag.clamps, "[Shape.button] Radius"));
    g_assert_true(has(t->unknown, "[Radius] bogus (c.ini)"));
    g_assert_true(has(t->unknown, "[Shape.nothing] Radius (c.ini)"));
    r4_theme_free(t);
}

/* A switch is a family of its own: the design's [Shape.switch] squares its track and knob, and a
 * design without that block keeps the library's pill (half the track height). */
static void test_switch_follows_design(void)
{
    put("pill.ini", "[Design]\nSchema=1\n");
    put("square.ini", "[Design]\nSchema=1\n[Shape.switch]\nRadius=0px\n");
    r4_geometry g;
    double w, h;
    r4_theme *t = load("[Design]\nName=pill\n");
    r4_family_box(&t->shape, R4_FAM_SWITCH, &w, &h);
    r4_design_geometry(&t->design, &t->shape, R4_FAM_SWITCH, w, h, &g, &t->diag);
    g_assert_cmpfloat(g.radius[0], ==, h / 2);
    r4_theme_free(t);
    t = load("[Design]\nName=square\n");
    r4_family_box(&t->shape, R4_FAM_SWITCH, &w, &h);
    r4_design_geometry(&t->design, &t->shape, R4_FAM_SWITCH, w, h, &g, &t->diag);
    g_assert_cmpfloat(g.radius[0], ==, 0);
    char *c = css_of(t);
    g_assert_nonnull(strstr(c, "solid @track; border-radius: 0px; color: @text;"));
    g_free(c);
    r4_theme_free(t);
}

static void test_fidelity_and_golden(void)
{
    put("ind.ini",
        "[Design]\nSchema=1\n[Shape.button]\nShape=chamfered\nChamfer=4px\n"
        "[Shape.menu]\nShape=cut\nCut=3px\n");
    r4_theme *t = load("[Design]\nName=ind\n");
    char *c = css_of(t);
    char *rep = r4_design_fidelity(&t->design, &t->shape, &t->diag);
    g_assert_nonnull(strstr(rep, "button     chamfered -> reduced 4px"));
    g_assert_true(has(t->diag.downgrades, "button: chamfered 4 px"));
    g_assert_true(has(t->diag.downgrades, "menu: cut 3 px"));
    g_free(rep);
    /* The same input twice gives the same bytes. */
    char *c2 = css_of(t);
    g_assert_cmpstr(c, ==, c2);
    char *d1 = r4_design_dump(&t->design, &t->shape), *d2 = r4_design_dump(&t->design, &t->shape);
    g_assert_cmpstr(d1, ==, d2);
    g_free(c);
    g_free(c2);
    g_free(d1);
    g_free(d2);
    r4_theme_free(t);

    /* The shipped soft design resolves to the committed dump. */
    char *dir = g_build_filename(g_getenv("G_TEST_SRCDIR"), "..", "data", "designs", NULL);
    g_setenv("RICE_GTK4_DESIGN_DIR", dir, TRUE);
    g_unsetenv("RICE_GTK4_DESIGN_PATH");
    g_setenv("RICE_GTK4_DESIGN", "soft", TRUE);
    put("gtk4.conf", "");
    char *cfg = g_build_filename(tmp, "gtk4.conf", NULL);
    t = r4_theme_load(cfg);
    char *dump = r4_design_dump(&t->design, &t->shape);
    char *gold_path = g_build_filename(g_getenv("G_TEST_SRCDIR"), "golden", "soft.dump", NULL);
    char *gold = NULL;
    g_assert_true(g_file_get_contents(gold_path, &gold, NULL, NULL));
    g_assert_cmpstr(dump, ==, gold);
    g_unsetenv("RICE_GTK4_DESIGN");
    g_free(gold);
    g_free(gold_path);
    g_free(dump);
    g_free(cfg);
    g_free(dir);
    r4_theme_free(t);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    tmp = g_dir_make_tmp("rice-gtk4-design-XXXXXX", NULL);
    g_test_add_func("/design/chain", test_chain);
    g_test_add_func("/design/config-over-design", test_config_over_design);
    g_test_add_func("/design/integer-only-toolkit", test_integer_only_toolkit);
    g_test_add_func("/design/cycle-and-missing", test_cycle_and_missing);
    g_test_add_func("/design/clamps-and-unknown", test_clamps_and_unknown);
    g_test_add_func("/design/switch-follows-design", test_switch_follows_design);
    g_test_add_func("/design/fidelity-and-golden", test_fidelity_and_golden);
    int r = g_test_run();
    g_rmdir(tmp); /* leftover files are in the system temp dir; the test never writes elsewhere */
    return r;
}
