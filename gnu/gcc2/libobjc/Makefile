#  GNU Objective C Runtime Makefile
#  Copyright (C) 1993 Free Software Foundation, Inc.
#
#  This file is part of GNU CC.
#
#  GNU CC is free software; you can redistribute it and/or modify it under the
#  terms of the GNU General Public License as published by the Free Software
#  Foundation; either version 2, or (at your option) any later version.
#
#  GNU CC is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
#  details.
#
#  You should have received a copy of the GNU General Public License along with
#  GNU CC; see the file COPYING.  If not, write to the Free Software
#  Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#  This makefile is run by the parent dir's makefile.
#  thisdir1=`pwd`; \
#  srcdir1=`cd $(srcdir); pwd`; \
#  cd objc; \
#  $(MAKE) $(MAKEFLAGS) -f $$srcdir1/objc/Makefile libobjc.a \
#    srcdir=$$srcdir1 tooldir=$(tooldir) AR="$(AR)" AR_FLAGS="$(AR_FLAGS)" \
#    GCC_FOR_TARGET="$$thisdir1/xgcc -B$$thisdir1/" \
#    GCC_CFLAGS="$(GCC_CFLAGS)" incinstalldir=$$thisdir1/include
#  Two targets are used by ../Makefile: `all' and `mostlyclean'.

.SUFFIXES: .m

OPTIMIZE= -O

VPATH = $(srcdir)/objc

AR = ar
AR_FLAGS = rc

# Always search these dirs when compiling.
SUBDIR_INCLUDES = -I. -I.. -I$(srcdir) -I$(srcdir)/config

.c.o:
	$(GCC_FOR_TARGET) $(OPTIMIZE) \
		-c $(GCC_CFLAGS) $(SUBDIR_INCLUDES) $<

.m.o:
	$(GCC_FOR_TARGET) $(OPTIMIZE) -fgnu-runtime \
		-c $(GCC_CFLAGS) $(SUBDIR_INCLUDES) $<

# If we were not invoked from the parent dir,
# invoke make in the parent dir and have reinvoke this makefile.
# That's necessary to get the right values for srcdir, etc.
all:
	cd ..; $(MAKE) sublibobjc.a

OBJC_O = hash.o sarray.o class.o sendmsg.o init.o archive.o \
         selector.o objects.o misc.o Object.o Protocol.o

libobjc.a: $(OBJC_O)
	-rm -f libobjc.a
	$(AR) rc libobjc.a $(OBJC_O)
# ranlib is run in the parent directory's makefile.

OBJC_H = hash.h list.h sarray.h objc.h \
         objc-api.h \
	 Object.h Protocol.h mutex.h \
	 typedstream.h

# copy objc headers to installation include directory
copy-headers: 
	-rm -fr $(incinstalldir)/objc
	-mkdir $(incinstalldir)/objc
	for file in $(OBJC_H); do \
	  realfile=$(srcdir)/objc/$$file; \
	  cp $$realfile $(incinstalldir)/objc; \
	  chmod a+r $(incinstalldir)/objc/$$file; \
	done

sendmsg.o: sendmsg.c fflags
	$(GCC_FOR_TARGET) `cat fflags` -c  $(GCC_CFLAGS) $(SUBDIR_INCLUDES) $(srcdir)/objc/sendmsg.c

## Next to are for heuristics on forwarding mechanism... 
xforward: xforward.c
	-$(GCC_FOR_TARGET) -c $(GCC_CFLAGS) $(SUBDIR_INCLUDES) $(srcdir)/objc/xforward.c
	-$(GCC_FOR_TARGET) ./xforward.o -o xforward
	if [ ! -f ./xforward ]; then touch ./xforward; else true; fi	

fflags: xforward
	-rm -f fflags
	-if [ -s ./xforward ]; then ./xforward > fflags; else touch fflags; fi

mostlyclean:
	-rm -f *.o libobjc.a xforward fflags
clean: mostlyclean
distclean: mostlyclean
extraclean: mostlyclean
