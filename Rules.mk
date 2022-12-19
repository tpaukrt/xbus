# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2022 Tomas Paukrt

# default subdirectory for object files
OBJDIR ?= OBJ

# default directories for installation
BINDIR ?= /usr/bin
LIBDIR ?= /usr/lib
INCDIR ?= /usr/include

# build verbosity
ifeq ($(V),1)
Q :=
E := @true
else
Q := @
E := @echo
endif

# build serialization
ifneq ($(filter clean%, $(MAKECMDGOALS)),)
.NOTPARALLEL:
endif

# default target
all: build

# target for building all binary files
build:

# target for installing all binary and header files
install:

# target for uninstalling all binary and header files
uninstall:

# target for cleaning all binary and temporary files
clean:
	$(Q) rm -rf $(firstword $(subst ., ,$(OBJDIR)))*

# function to generate rules for building, installing and uninstalling a target binary file
define generate-rules
build: build-$(1)

install: install-$(1)

uninstall: uninstall-$(1)

build-$(1): $(OBJDIR)/$(1)/$($(1)_FILE)

install-$(1): build-$(1)
	$(Q) $($(1)_PRE_INSTALL)
	$(if $($(1)_PATH),
	  $(E) "  CP  $(1)/$($(1)_FILE)"
	  $(Q) install -D -m $($(1)_MODE) $(OBJDIR)/$(1)/$($(1)_FILE) $(DESTDIR)$($(1)_PATH)/$($(1)_FILE)
	  $(foreach FILE, $($(1)_HEADER_LIST),
	    $(E) "  CP  $(1)/$(FILE)"
	    $(Q) install -D -m 644 $(1)/$(FILE) $(DESTDIR)$($(1)_HEADER_PATH)/$(FILE)
	  )
	)
	$(Q) $($(1)_INSTALL)
	$(Q) $($(1)_POST_INSTALL)

uninstall-$(1):
	$(Q) $($(1)_PRE_UNINSTALL)
	$(Q) $($(1)_UNINSTALL)
	$(if $($(1)_PATH),
	  $(E) "  RM  $(1)/$($(1)_FILE)"
	  $(Q) rm -f $(DESTDIR)$($(1)_PATH)/$($(1)_FILE)
	  $(foreach FILE, $($(1)_HEADER_LIST),
	    $(E) "  RM  $(1)/$(FILE)"
	    $(Q) rm -f $(DESTDIR)$($(1)_HEADER_PATH)/$(FILE)
	  )
	)
	$(Q) $($(1)_POST_UNINSTALL)

clean-$(1):
	$(Q) rm -rf $(OBJDIR)/$(1)

$($(1)_OBJECT_DIRS):
	$(Q) mkdir -p $$@

$(OBJDIR)/$(1)/%.o: $(1)/%.c $(DEPENDS) | $($(1)_OBJECT_DIRS)
	$(E) "  CC  $$(patsubst $(OBJDIR)/%,%,$$@)"
	$(Q) $(CCACHE) $($(1)_COMPILER) -MMD -MP $(CPPFLAGS) $($(1)_CPPFLAGS) -fPIC $(CFLAGS) $($(1)_CFLAGS) -c $$< -o $$@

$(OBJDIR)/$(1)/%.o: $(1)/%.cc $(DEPENDS) | $($(1)_OBJECT_DIRS)
	$(E) "  CC  $$(patsubst $(OBJDIR)/%,%,$$@)"
	$(Q) $(CCACHE) $($(1)_COMPILER) -MMD -MP $(CPPFLAGS) $($(1)_CPPFLAGS) -fPIC $(CXXFLAGS) $($(1)_CXXFLAGS) -c $$< -o $$@

$(OBJDIR)/$(1)/%.o: $(1)/%.cpp $(DEPENDS) | $($(1)_OBJECT_DIRS)
	$(E) "  CC  $$(patsubst $(OBJDIR)/%,%,$$@)"
	$(Q) $(CCACHE) $($(1)_COMPILER) -MMD -MP $(CPPFLAGS) $($(1)_CPPFLAGS) -fPIC $(CXXFLAGS) $($(1)_CXXFLAGS) -c $$< -o $$@

$(OBJDIR)/$(1)/$($(1)_NAME): $($(1)_OBJECT_LIST)
	$(E) "  LD  $$(patsubst $(OBJDIR)/%,%,$$@)"
	$(Q) $(CCACHE) $($(1)_LINKER) -pie $(LDFLAGS) $($(1)_LDFLAGS) -o $$@ $$(filter %.o, $$^) $($(1)_LDLIBS) $(LDLIBS)

$(OBJDIR)/$(1)/$($(1)_NAME).so: $($(1)_OBJECT_LIST)
	$(E) "  LD  $$(patsubst $(OBJDIR)/%,%,$$@)"
	$(Q) $(CCACHE) $($(1)_LINKER) -shared $(LDFLAGS) $($(1)_LDFLAGS) -o $$@ $$(filter %.o, $$^) $($(1)_LDLIBS) $(LDLIBS)

$(OBJDIR)/$(1)/$($(1)_NAME).a: $($(1)_OBJECT_LIST)
	$(E) "  AR  $$(patsubst $(OBJDIR)/%,%,$$@)"
	$(Q) rm -f $$@
	$(Q) $(AR) rcs $$@ $$(filter %.o, $$^)

-include $($(1)_DEPEND_LIST)
endef

# auxiliary function to set basic properties of a target binary file
define set-properties
$(eval $(1)_NAME ?= $(1))
$(eval $(1)_PATH ?= $(2))
$(eval $(1)_MODE ?= $(3))
$(eval $(1)_FILE := $($(1)_NAME)$(4))
$(eval $(1)_HEADER_PATH ?= $(INCDIR))
$(eval $(1)_HEADER_LIST ?= $(if $(4), $(foreach EXT, h hh hpp, $(patsubst $(1)/%, %, $(foreach DIR, $(1) $(1)/*, $(wildcard $(DIR)/*.$(EXT)))))))
$(eval $(1)_SOURCE_LIST ?= $(foreach EXT, c cc cpp, $(patsubst $(1)/%, %, $(foreach DIR, $(1) $(1)/*, $(sort $(wildcard $(DIR)/*.$(EXT)))))))
$(eval $(1)_DEPEND_LIST := $(foreach EXT, c cc cpp, $(patsubst %.$(EXT), $(OBJDIR)/$(1)/%.d, $(filter %.$(EXT), $($(1)_SOURCE_LIST)))))
$(eval $(1)_OBJECT_LIST := $(foreach EXT, c cc cpp, $(patsubst %.$(EXT), $(OBJDIR)/$(1)/%.o, $(filter %.$(EXT), $($(1)_SOURCE_LIST)))))
$(eval $(1)_OBJECT_DIRS := $(sort $(dir $(addprefix $(OBJDIR)/$(1)/, $($(1)_SOURCE_LIST)))))
$(eval $(1)_COMPILER    ?= $(if $(filter %.cc %.cpp, $($(1)_SOURCE_LIST)), $(CXX), $(CC)))
$(eval $(1)_LINKER      ?= $($(1)_COMPILER))
endef

# auxiliary function to set dependencies of a target binary file on internal and external libraries
define set-dependencies
$(foreach LIB, $(filter $(wildcard *), $($(1)_LIBS)),
  $(OBJDIR)/$(1)/$($(1)_FILE) : $(OBJDIR)/$(LIB)/$($(LIB)_FILE)
  $(eval $(1)_CPPFLAGS += -I$(LIB))
  $(eval $(1)_LDFLAGS  += -L$(OBJDIR)/$(LIB))
  $(eval $(1)_LDLIBS   += $(patsubst lib%, -l%, $($(LIB)_NAME)))
)
$(foreach LIB, $(filter-out $(wildcard *), $($(1)_LIBS)),
  $(eval $(1)_LDLIBS   += $(patsubst lib%, -l%, $(LIB)))
)
endef

# generate auxiliary variables and build rules for each target binary file
$(foreach DIR, $(PROGRAMS), $(eval $(call set-properties,$(DIR),$(BINDIR),755)))
$(foreach DIR, $(SHARED_LIBS), $(eval $(call set-properties,$(DIR),$(LIBDIR),755,.so)))
$(foreach DIR, $(STATIC_LIBS), $(eval $(call set-properties,$(DIR),$(LIBDIR),644,.a)))
$(foreach DIR, $(STATIC_LIBS) $(SHARED_LIBS) $(PROGRAMS), $(eval $(call set-dependencies,$(DIR))))
$(foreach DIR, $(STATIC_LIBS) $(SHARED_LIBS) $(PROGRAMS), $(eval $(call generate-rules,$(DIR))))
