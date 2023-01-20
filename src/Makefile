
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use

# Required compiler parameters
REQCFLAGS = -I../libmseed

BIN = msi

LDFLAGS += -L../libmseed
LDLIBS += -lmseed

OBJS = $(BIN).o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o ../$@ $(OBJS) $(LDLIBS) $(LDFLAGS)

clean:
	rm -f $(OBJS) ../$(BIN)

# Implicit rule for building object files
%.o: %.c
	$(CC) $(CFLAGS) $(REQCFLAGS) -c $<

install:
	@echo
	@echo "No install target, copy the executable and documentation to desired location"
	@echo
