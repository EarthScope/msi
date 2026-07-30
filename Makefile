# Build environment can be configured with the following
# variables, set in the environment or on the command line,
# e.g. make CFLAGS="-g -Wall":
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use (default: -O2)
#   LDFLAGS : Specify linker options to use
#   WITHOUTURL : Set to any value to disable URL support via libcurl

# Recommended default optimization level (overridable on command line)
CFLAGS ?= -O2

# Automatically configure URL support if libcurl is present
# Test for curl-config command and add build options if found
# Prefer /usr/bin/curl-config over any other curl-config
ifndef WITHOUTURL
  ifneq (,$(wildcard /usr/bin/curl-config))
     CURL_CONFIG := /usr/bin/curl-config
  else ifneq (,$(shell command -v curl-config))
     CURL_CONFIG := $(shell command -v curl-config)
  endif
endif

ifneq (,$(CURL_CONFIG))
  export LM_CURL_VERSION=$(shell $(CURL_CONFIG) --version)
  EXTRA_CFLAGS := -DLIBMSEED_URL
  EXTRA_LDFLAGS := $(shell $(CURL_CONFIG) --libs)
  $(info Configured with $(LM_CURL_VERSION))
endif

# Variables passed to sub-makes
SUBMAKE_ARGS = CFLAGS="$(CFLAGS) $(EXTRA_CFLAGS)" LDFLAGS="$(LDFLAGS) $(EXTRA_LDFLAGS)"

.PHONY: all clean
all clean:
	$(MAKE) -C libmseed $@ $(SUBMAKE_ARGS)
	$(MAKE) -C src $@ $(SUBMAKE_ARGS)

.PHONY: test
test: all
	@test/run-tests.py

.PHONY: install
install:
	@echo
	@echo "No install method"
	@echo "Copy the binary and documentation to desired location"
	@echo
