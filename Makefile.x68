# for x68000 GCCê^óùéqî≈
CC = gcc
CFLAGS = -O -DALLMEM -DBIG_ENDIAN -cpp-stack=409600 -IB:/INCLUDE
LIBDIR = B:\LIB
LIBS = $(addprefix $(LIBDIR)/, libz.a libpng.a FLOATFNC.L)

LDFLAGS = -cpp-stack=409600
INCLUDE = -I.\

.PHONY: clean

EXECUTABLE = png2chp16.x

all: $(EXECUTABLE)

OBJS = png2chp16.o pngctrl.o

$(EXECUTABLE): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c pngctrl.h
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

clean:
	rm -f $(EXECUTABLE) $(OBJS)
