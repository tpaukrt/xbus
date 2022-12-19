# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2022 Tomas Paukrt

# ccache command
CCACHE   := $(shell command -v ccache 2> /dev/null)

# subdirectory for object files
OBJDIR   := OBJ

# basic compiler flags
CFLAGS   += -Wall -Wextra -Wshadow -Wmissing-declarations -Wformat-security

# extra compiler flags for debug and release build
ifeq ($(DEBUG),1)
OBJDIR   := $(OBJDIR).debug
CFLAGS   += -O0 -ggdb
CPPFLAGS += -DDEBUG
else
OBJDIR   := $(OBJDIR).release
CFLAGS   += -O2
LDFLAGS  += -s
endif

# extra compiler flags for address sanitizer
ifeq ($(ASAN),1)
OBJDIR   := $(OBJDIR).asan
CFLAGS   += -fsanitize=address -fno-omit-frame-pointer
LDFLAGS  += -fsanitize=address
endif

# extra compiler flags for undefined behavior sanitizer
ifeq ($(UBSAN),1)
OBJDIR   := $(OBJDIR).ubsan
CFLAGS   += -fsanitize=undefined -fno-omit-frame-pointer
LDFLAGS  += -fsanitize=undefined
endif
