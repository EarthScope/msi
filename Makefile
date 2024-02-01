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
  export CFLAGS:=$(CFLAGS) -DLIBMSEED_URL
  export LDFLAGS:=$(LDFLAGS) $(shell $(CURL_CONFIG) --libs)
  $(info Configured with $(LM_CURL_VERSION))
endif

.PHONY: all clean
all clean: libmseed
	$(MAKE) -C src $@

.PHONY: libmseed
libmseed:
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: install
install:
	@echo
	@echo "No install method"
	@echo "Copy the binary and documentation to desired location"
	@echo
