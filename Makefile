# rice-gtk4 - GTK 4 theme generator (C, Meson).
# Needs rice-colors and rice-ini installed (found with pkg-config); set PKG_CONFIG_PATH
# if they live outside the default search path. `make help` lists the targets.

PREFIX ?= $(HOME)/.local
BUILD_DIR ?= .build
MESON ?= meson
CLANG_FORMAT ?= clang-format
CLANG_TIDY ?= clang-tidy
BEAR ?= bear

SOURCES := $(sort $(wildcard src/*/*.c src/*/*.h tests/*.c tests/*.h))
VERSION := $(shell cat VERSION)

.DEFAULT_GOAL := help
.PHONY: help build test check install uninstall fmt format-check lint compdb clean

help:
	@printf '%s\n' \
		'rice-gtk4 $(VERSION)' \
		'  build         Configure and compile into $(BUILD_DIR)/' \
		'  test          Run the tests' \
		'  check         format-check, test, lint' \
		'  install       Install under PREFIX (default ~/.local); DESTDIR is honored' \
		'  uninstall     Remove what install put under PREFIX' \
		'  fmt           Format sources with clang-format' \
		'  format-check  Fail if a source is not formatted' \
		'  lint          Run clang-tidy over src/ (writes compile_commands.json first)' \
		'  compdb        Write compile_commands.json with bear (for clangd and clang-tidy)' \
		'  clean         Remove $(BUILD_DIR)/ and compile_commands.json' \
		'Variables: PREFIX BUILD_DIR DESTDIR PKG_CONFIG_PATH'

$(BUILD_DIR)/build.ninja:
	$(MESON) setup "$(BUILD_DIR)" --prefix "$(PREFIX)" --buildtype=debug

build: $(BUILD_DIR)/build.ninja
	$(MESON) compile -C "$(BUILD_DIR)"

test: build
	$(MESON) test -C "$(BUILD_DIR)" --print-errorlogs

install: build
	$(MESON) configure "$(BUILD_DIR)" --prefix "$(PREFIX)"
	$(MESON) install -C "$(BUILD_DIR)"

uninstall: $(BUILD_DIR)/build.ninja
	ninja -C "$(BUILD_DIR)" uninstall

fmt:
	$(CLANG_FORMAT) -i $(SOURCES)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(SOURCES)

compdb: $(BUILD_DIR)/build.ninja
	ninja -C "$(BUILD_DIR)" -t clean >/dev/null
	$(BEAR) --output compile_commands.json -- $(MESON) compile -C "$(BUILD_DIR)"

lint: compdb
	$(CLANG_TIDY) --config-file=.clang-tidy -p . $(sort $(wildcard src/*/*.c))

check: format-check test lint

clean:
	rm -rf -- "$(BUILD_DIR)" compile_commands.json
