
DIRS = libmseed src

# Automatically configure URL support if libcurl is present
# Test for curl-config command and add build options if so
ifneq (,$(shell command -v curl-config))
        export LM_CURL_VERSION=$(shell curl-config --version)
        export CFLAGS:=$(CFLAGS) -DLIBMSEED_URL
        export LDFLAGS:=$(LDFLAGS) $(shell curl-config --libs)
        $(info Configured with $(LM_CURL_VERSION))
endif

all clean install ::
	@for d in $(DIRS) ; do \
	    echo "Running $(MAKE) $@ in $$d" ; \
	    if [ -f $$d/Makefile -o -f $$d/makefile ] ; \
	        then ( cd $$d && $(MAKE) $@ ) ; \
	    elif [ -d $$d ] ; \
	        then ( echo "ERROR: no Makefile/makefile in $$d for $(CC)" ) ; \
	    fi ; \
	done
