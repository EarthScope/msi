# Automatically configure URL support if libcurl is present
# Test for curl-config command and add build options if so
ifneq (,$(shell command -v curl-config))
        export LM_CURL_VERSION=$(shell curl-config --version)
        export CFLAGS:=$(CFLAGS) -DLIBMSEED_URL
        export LDFLAGS:=$(LDFLAGS) $(shell curl-config --libs)
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
	@echo%