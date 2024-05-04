#
# Linux bsd Makefile
#
# Copyright (C) 2019, Broadcom. All Rights Reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# <<Broadcom-WL-IPTag/Open:>>
#
# $Id: Makefile 401201 2013-05-09 03:06:42Z $
#

CFLAGS	+= -I. -I$(TOP)/shared -I$(TOP)/bshared/include -I$(SRCBASE)/include -I$(SRCBASE)/../components/wlioctl/include -I$(SRCBASE)/../components/proto/include -I$(BASEDIR)/components/apps/appeventd -Wall -Werror
CFLAGS	+= $(if $(WLAN_ComponentIncPath),$(WLAN_ComponentIncPath),$(addprefix -I,$(wildcard $(SRCBASE)/shared/bcmwifi/include)))

CFLAGS += -I$(SRCBASE)/../components/math/include
CFLAGS += -I$(SRCBASE)/wl/sys

#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s -O2
LDFLAGS += -L$(TOP)/nvram -L$(INSTALLDIR)/nvram/usr/lib -lnvram
LDFLAGS += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared -lpthread
LDFLAGS += $(EXTRA_LDFLAGS)

vpath %.c $(SRCBASE)/shared $(SRCBASE)/shared/bcmwifi/src $(WLAN_ComponentSrcDirs)

all: bsd

OBJS = bsd_main.o bsd.o bsd_engine.o  bsd_util.o bsd_comm.o bcmwifi_channels.o bsd_default.o

install: all
	install -d $(INSTALLDIR)/usr/sbin/
	install bsd $(INSTALLDIR)/usr/sbin/
	$(STRIP) $(INSTALLDIR)/usr/sbin/bsd

clean:
	rm -f *.o *.a *.so bsd

${OBJS}: bsd.h

bsd: bcmutils.o ${OBJS}
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS)
