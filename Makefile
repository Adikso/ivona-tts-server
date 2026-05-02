ifeq ($(origin CC),default)
  CC := x86_64-w64-mingw32-gcc
endif

CFLAGS  ?= -O2 -Wall -Wextra \
           -Wno-cast-function-type \
           -Wno-unterminated-string-initialization \
           -Wno-format-truncation
LDFLAGS ?= -static

SRCDIR    := src
BUILDDIR  := build
FCGI_DIR  := $(BUILDDIR)/fcgi-mingw
FCGI_LIB  := $(FCGI_DIR)/lib/libfcgi.a

DLL    := $(BUILDDIR)/ivona.dll
IMPLIB := $(BUILDDIR)/libivona.a
EXES   := $(BUILDDIR)/tts2wav.exe $(BUILDDIR)/tts_server.exe

.PHONY: all fcgi clean clean-fcgi
all: $(DLL) $(EXES)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

fcgi $(FCGI_LIB):
	./scripts/setup-fcgi.sh $(FCGI_DIR)

$(DLL) $(IMPLIB): $(SRCDIR)/ivona.c $(SRCDIR)/ivona.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -shared $(SRCDIR)/ivona.c \
	      -Wl,--out-implib,$(IMPLIB) -o $(DLL) $(LDFLAGS)

$(BUILDDIR)/tts2wav.exe: $(SRCDIR)/tts2wav.c $(SRCDIR)/ivona.h $(IMPLIB) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -municode $(SRCDIR)/tts2wav.c \
	      -L$(BUILDDIR) -livona -o $@ $(LDFLAGS)

$(BUILDDIR)/tts_server.exe: $(SRCDIR)/tts_server.c $(SRCDIR)/ivona.h $(IMPLIB) $(FCGI_LIB) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(FCGI_DIR)/include $(SRCDIR)/tts_server.c \
	      -L$(BUILDDIR) -livona -L$(FCGI_DIR)/lib -lfcgi -lws2_32 -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)

clean-fcgi:
	rm -rf $(FCGI_DIR)
