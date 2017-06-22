#
# Makefile	Radius Makefile for BSD (FreeBSD, NetBSD, etc)
#
#

CC	= gcc
CFLAGS	= -g -O -Wall -DNOSHADOW -I/usr/include -I/usr/local/include
LDFLAGS	= # -s
LIBS	=
#LCRYPT	= -lcrypt -lcrypto -L/usr/local/lib
#LCRYPT	= -lcrypt -lcrypto -ldescrypt
LCRYPT	= -lcrypt -lcrypto

# Set these to the correct values if you want DBM support.
#DBM     = -DUSE_DB1
#DBMLIB  = #-ldb

#PAM	= -DPAM
#PAMLIB	= -lpam

# mysql
MYSQL  = -DUSEMYSQL -DCHAP -DCHAPMS -DCHAPMS2
#MYSQL  += -DCREATE_NAME_TABLE
#MYSQL  = -DUSEMYSQL
MYSQLLIB = -lmysqlclient -L/usr/local/lib/mysql

INSTALL = install
DINSTALL = mkdir -p

BINDIR  = /usr/local/bin
SBINDIR = /usr/local/sbin
MANDIR  = /usr/local/man

.include "Make.inc"
