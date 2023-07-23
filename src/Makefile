
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use

BIN = msi

SRCS = msi.c
OBJS = $(SRCS:.c=.o)

# Required compiler parameters
EXTRACFLAGS = -I../libmseed
EXTRALDFLAGS = -L../libmseed

LDLIBS = -lmseed

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o ../$@ $(OBJS) $(EXTRALDFLAGS) $(LDLIBS) $(LDFLAGS)

clean:
	rm -f $(OBJS) ../$(BIN)

# Implicit rule for building object files
%.o: %.c
	$(CC) $(CFLAGS) $(EXTRACFLAGS) -c $<

install:
	@echo
	@echo "No install target, copy the executable and documentation as needed"
	@echo
