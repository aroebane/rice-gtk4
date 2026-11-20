/* rice-gtk4 core: the element manifest (data/element-hooks.yaml) as a table, for [Element.<id>].
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RICEGTK4_ELEMENTS_H
#define RICEGTK4_ELEMENTS_H

#include <stddef.h>

typedef struct {
    const char *id;
    const char *const *hooks; /* CSS selectors from the manifest, NULL terminated */
} r4_element_def;

extern const r4_element_def r4_element_defs[];
extern const unsigned r4_element_def_count;

/* The manifest entry named id, or NULL. */
const r4_element_def *r4_element_find(const char *id);

#endif
