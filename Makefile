# Sraz (native, SDL2): one Makefile for Windows, Linux and macOS, run from the repository root.
#
#   Windows  MSYS2 MinGW64 gcc, make, pkg-config, SDL2:   make
#   Linux    gcc, libsdl2-dev, pkg-config:                make
#   macOS    Xcode tools, SDL2 (brew install sdl2):       make
#
#   make          build sraz (sraz.exe on Windows)
#   make test     build and run the rules, LAN, flow and rasteriser tests
#   make clean    remove build outputs
#
# On Windows SDL2 is linked statically, so the program starts without any DLL.

ifeq ($(origin CC),default)
  CC := gcc
endif
UNAME := $(shell uname -s 2>/dev/null)
# another folder keeps two systems' builds apart in one checkout
BUILD   ?= build
VERSION ?= $(shell head -1 VERSION 2>/dev/null || echo 0.0)

# c99 rather than gnu99 keeps the code portable; the systems' POSIX and Win32 calls are switched on by name below.
DEPFLAGS ?= -MMD -MP
CFLAGS  := -std=c99 -O2 -g -Wall -Wextra -pedantic -Wdeclaration-after-statement $(DEPFLAGS) -I$(BUILD) \
           -DSRAZ_VERSION='"$(VERSION)"'

SRC := $(addprefix src/,main.c app.c ui.c ui_game.c gfx.c text.c vg.c png.c strs.c game.c snd.c board.c quiz.c lan.c)

# Linux and macOS: SDL_CONFIG names an sdl2-config (for a static SDL2 built from source), otherwise pkg-config is used;
# SDL_LIBS may also be given on the command line.
ifneq ($(SDL_CONFIG),)
  SDL_CFLAGS ?= $(shell $(SDL_CONFIG) --cflags)
  SDL_LIBS   ?= $(shell $(SDL_CONFIG) --static-libs)
else
  SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2)
  SDL_LIBS   ?= $(shell pkg-config --libs sdl2)
endif

ON_WINDOWS := $(filter Windows_NT,$(OS))$(findstring MINGW,$(UNAME))$(findstring MSYS,$(UNAME))
ifneq ($(ON_WINDOWS),)
  X       := .exe
  # msvcrt's printf has no %zu or %lld; this makes MinGW use its own conforming one
  CFLAGS  += -D__USE_MINGW_ANSI_STDIO=1 -D_WIN32_WINNT=0x0601
  # main() stays main(): SDL2main is not linked, so SDL must not rename it, and it is told the entry is handled
  CFLAGS  += -DSDL_MAIN_HANDLED $(filter-out -Dmain=SDL_main,$(shell pkg-config --cflags sdl2))
  LDFLAGS := -mwindows
  SDL_LIBS := $(filter-out -lSDL2main -mwindows -lmingw32,$(shell pkg-config --static --libs sdl2))
  LIBS    := $(SDL_LIBS) -lws2_32 -static -static-libgcc
  # MSYS2 make does not always pass TMP / TEMP on, and gcc then tries a system folder; keep its temp files here
  $(shell mkdir -p $(BUILD)/tmp)
  export TMP := $(CURDIR)/$(BUILD)/tmp
  export TEMP := $(TMP)
  # The compiler's own folder first on PATH: other toolchains can ship an incompatible libmpfr that makes cc1 die
  # silently.
  CC_BIN := $(shell command -v $(CC) 2>/dev/null)
  ifneq ($(CC_BIN),)
    CC_DIR := $(patsubst %/,%,$(dir $(CC_BIN)))
    export PATH := $(CC_DIR):$(PATH)
  endif
  TEST_LIBS := -lws2_32
else ifeq ($(UNAME),Darwin)
  X       :=
  CFLAGS  += $(SDL_CFLAGS)
  LIBS    := $(SDL_LIBS) -lm
  TEST_LIBS :=
else
  X       :=
  # glibc hides POSIX calls under -std=c99 without these
  CFLAGS  += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE $(SDL_CFLAGS)
  LIBS    := $(SDL_LIBS) -lm
  TEST_LIBS :=
endif

EXE ?= sraz$(X)
OBJ := $(patsubst %.c,$(BUILD)/%.o,$(SRC)) $(BUILD)/stb_impl.o $(BUILD)/gen_font.o

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# third-party code: warnings off
$(BUILD)/stb_impl.o: src/stb_impl.c third_party/stb/stb_truetype.h
	@mkdir -p $(BUILD)
	$(CC) -std=c99 -O2 -w -c $< -o $@

# The font built in as a C array ("ld -r -b binary" is GNU ld only). HOSTCC: bin2c runs here even when CC cross-builds.
HOSTCC ?= cc
ifneq ($(ON_WINDOWS),)
  HOSTCC := $(CC)
endif
ifeq ($(UNAME),Darwin)
  HOSTCC := clang
endif
$(BUILD)/bin2c$(X): tools/bin2c.c
	@mkdir -p $(BUILD)
	$(HOSTCC) -O1 -o $@ $<

$(BUILD)/gen_font.c: fonts/DroidSans.ttf $(BUILD)/bin2c$(X)
	$(BUILD)/bin2c$(X) $< $@ font_ttf

$(BUILD)/gen_font.o: $(BUILD)/gen_font.c
	$(CC) -O1 -w -c $< -o $@

# ---- tests: none of them needs SDL or a display. They read questions/ from the repository root, where make is run.
TESTS := $(BUILD)/test_board$(X) $(BUILD)/test_lan$(X) $(BUILD)/test_game$(X) $(BUILD)/test_vg$(X)

$(BUILD)/test_board$(X): src/test_board.c src/board.c src/board.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ src/test_board.c src/board.c

$(BUILD)/test_lan$(X): src/test_lan.c src/lan.c src/lan.h src/board.c src/board.h src/quiz.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ src/test_lan.c src/board.c $(TEST_LIBS)

$(BUILD)/test_game$(X): src/test_game.c src/game.c src/game.h src/board.c src/quiz.c src/lan.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ src/test_game.c src/game.c src/board.c src/quiz.c src/lan.c $(TEST_LIBS)

$(BUILD)/test_vg$(X): src/test_vg.c src/vg.c src/vg.h src/png.c src/png.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ src/test_vg.c src/vg.c src/png.c -lm

test: $(TESTS)
	$(BUILD)/test_board$(X)
	$(BUILD)/test_lan$(X)
	$(BUILD)/test_game$(X)
	$(BUILD)/test_vg$(X)

clean:
	rm -rf $(BUILD) $(EXE)

.PHONY: all test clean

-include $(OBJ:.o=.d)
