# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2022 Tomas Paukrt

# list of static libraries to be built
STATIC_LIBS = library

# list of programs to be built
PROGRAMS = server tool

# list of additional dependencies
DEPENDS = Makefile Setup.mk

# target specific settings
library_NAME = libxbus
server_NAME = xbusd
tool_NAME = xbus
tool_LIBS = library

# build setup and rules
include Setup.mk
include Rules.mk
