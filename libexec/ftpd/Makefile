#	@(#)Makefile	5.15 (Berkeley) 7/1/90


PROG=	ftpd
CFLAGS+=-I${.CURDIR}/../../usr.bin/ftp -DSETPROCTITLE
SRCS=	ftpd.c ftpcmd.c glob.c logwtmp.c popen.c vers.c
MAN8=	ftpd.8
CLEANFILES+=ftpcmd.c y.tab.h
.PATH:	${.CURDIR}/../../usr.bin/ftp

.if exists(/usr/lib/libcrypt.a)
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif


.include <bsd.prog.mk>
