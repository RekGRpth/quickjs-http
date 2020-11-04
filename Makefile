OBJDIR=.obj

CC=gcc
#-flto 
#CFLAGS=-g -O2 -Wall -MMD -MF $(OBJDIR)/$(@F).d
#CFLAGS += -Wno-array-bounds -Wno-format-truncation
#AR=gcc-ar
#STRIP=strip
LDFLAGS=-g
SHLIB=libhttp.so

PROGS=$(SHLIB)

all: $(PROGS)

LIB_OBJS=$(OBJDIR)/http.o

#LIBS=-lm -ldl -lrt -lhttp_parser -pthread

$(OBJDIR): Makefile
	mkdir -p $(OBJDIR)

$(SHLIB): $(OBJDIR) $(LIB_OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $(LIB_OBJS) $(LIBS)
#	$(STRIP) $@

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -fPIC -DJS_SHARED_LIBRARY -c -o $@ $<

clean:
	rm -rf $(OBJDIR)/ $(PROGS)

#test: all
