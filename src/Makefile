#
# Makefile	Radius Makefile for Linux (2.2/2.4, libc6 aka glibc)
#
#

#
#	Autoselect -lshadow and -lcrypt
#
ifneq ($(wildcard /usr/lib/libshadow.a),)
LSHADOW	= -lshadow
endif
ifneq ($(wildcard /usr/lib/libcrypt.a),)
LCRYPT	= -lcrypt
endif

CC	= gcc
CFLAGS	= -Wall -g # -DNOSHADOW
LDFLAGS	=
LIBS	= $(LSHADOW)

# Set these to the correct values if you want DBM support.
# Values are USE_DBM, USE_NDBM, USE_GDBM, USE_DB1, USE_DB2, USE_DB3
#            -ldbm    -lnbdm    -lgdbm    -ldb     -ldb     -ldb
#DBM	= -DUSE_DB3
#DBMLIB	= -ldb

# Uncomment these if you want PAM support
#PAM	= -DPAM
#PAMLIB	= -lpam -ldl

INSTALL = install
DINSTALL = install -d

BINDIR  = /usr/local/bin
SBINDIR = /usr/local/sbin
MANDIR  = /usr/local/man

include Make.inc
