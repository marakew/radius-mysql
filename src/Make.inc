#
# Makefile	RADIUS - 
#		Remote Authentication Dial In User Service
#
#
DIRLOG	 = /var/log

RADIUS_DIR    = /etc/raddb

SERVER_OBJS   = radiusd.o files.o acct.o version.o proxy.o \
			exec.o auth.o timestr.o readusers.o mysql.o chap.o
#SERVER_OBJS   = radiusd.o files.o acct.o pam.o version.o proxy.o \
#			exec.o auth.o timestr.o cache.o readusers.o mysql.o
LIB_OBJS      = dict.o util.o md5.o md4.o attrprint.o radius.o log.o pair.o encrattr.o

INCLUDES       = radius.h conf.h

all:	radiusd radwho radzap raduse radclient radrelay builddbm

dbm:
	@echo
	@echo "Make sure the DBM= and DBMLIB= lines are set up correctly in the Makefile,"
	@echo "then just type \"make\"."
	@echo

radiusd: $(SERVER_OBJS) $(LIB_OBJS)
	$(CC) $(LDFLAGS) -o radiusd $(SERVER_OBJS) \
	$(LIB_OBJS) \
	$(LIBS) $(LCRYPT) $(PAMLIB) $(DBMLIB) $(MYSQLLIB)

radiusd.o: radiusd.c $(INCLUDES)
	$(CC) $(CFLAGS) $(DBM) $(MYSQL) -c radiusd.c

acct.o: acct.c $(INCLUDES)
	$(CC) $(CFLAGS) $(MYSQL) -c acct.c

log.o: log.c $(INCLUDES)
	$(CC) $(CFLAGS) -c log.c

attrprint.o: attrprint.c $(INCLUDES)
	$(CC) $(CFLAGS) -c attrprint.c

dict.o: dict.c $(INCLUDES)
	$(CC) $(CFLAGS) -c dict.c

files.o: files.c $(INCLUDES)
	$(CC) $(CFLAGS) $(DBM) $(PAM) $(MYSQL) -c files.c

radius.o: radius.c $(INCLUDES)
	$(CC) $(CFLAGS) $(MYSQL) -c radius.c

readusers.o: readusers.c $(INCLUDES)
	$(CC) $(CFLAGS) -c readusers.c

util.o: util.c $(INCLUDES)
	$(CC) $(CFLAGS) -c util.c

#pam.o:  pam.c $(INCLUDES)
#	$(CC) $(CFLAGS) $(PAM) -c pam.c

mysql.o: mysql.c $(INCLUDES)
	$(CC) $(CFLAGS) $(MYSQL) -c mysql.c

chap.o: chap.c $(INCLUDES)
	$(CC) $(CFLAGS) $(MYSQL) -c chap.c

#cache.o:  cache.c $(INCLUDES)
#	$(CC) $(CFLAGS) -c cache.c 

proxy.o:  proxy.c $(INCLUDES)
	$(CC) $(CFLAGS) -c proxy.c

exec.o:  exec.c $(INCLUDES)
	$(CC) $(CFLAGS) -c exec.c

auth.o:  auth.c $(INCLUDES)
	$(CC) $(CFLAGS) $(PAM) $(MYSQL) -c auth.c

pair.o: pair.c $(INCLUDES)
	$(CC) $(CFLAGS) $(PAM) -c pair.c

version.o: version.c $(INCLUDES)
	$(CC) $(CFLAGS) $(DBM) $(PAM) -o version.o -c version.c

encrattr.o: encrattr.c $(INCLUDES)
	$(CC) $(CFLAGS) -c encrattr.c

radclient: radclient.o $(LIB_OBJS)
	$(CC) $(LDFLAGS) -o radclient radclient.o \
		$(LIB_OBJS) $(LIBS)

radclient.o: radclient.c $(INCLUDES)
	$(CC) $(CFLAGS) -c radclient.c

radrelay: radrelay.o $(LIB_OBJS)
	$(CC) $(LDFLAGS) -o radrelay radrelay.o \
		$(LIB_OBJS) $(LIBS)

radrelay.o: radrelay.c $(INCLUDES)
	$(CC) $(CFLAGS) -c radrelay.c

md5.o: md5.c md5.h
	$(CC) $(CFLAGS) -c md5.c

md4.o: md4.c md4.h
	$(CC) $(CFLAGS) -c md4.c

#deskey.o: deskey.c des.h
#	$(CC) $(CFLAGS) -c deskey.c

#desport.o: desport.c des.h
#	$(CC) $(CFLAGS) -c desport.c

builddbm: builddbm.o readusers.o $(LIB_OBJS)
	test -z "$(DBM)" || $(CC) $(LDFLAGS) -o builddbm \
		builddbm.o readusers.o \
		$(LIB_OBJS) $(DBMLIB) $(LIBS)

builddbm.o: builddbm.c
	test -z "$(DBM)" || $(CC) $(CFLAGS) -c $(DBM) builddbm.c

radwho: radwho.o $(LIB_OBJS)
	$(CC) $(LDFLAGS) -o radwho radwho.o \
		$(LIB_OBJS) $(LIBS)

raduse: raduse.o
	$(CC) $(LDFLAGS) -o raduse raduse.o $(LIBS)

radzap: radzap.o $(LIB_OBJS)
	$(CC) $(LDFLAGS) -o radzap radzap.o \
		$(LIB_OBJS) $(LIBS)

clean:
	rm -f *.o radiusd radwho raduse radclient radzap radrelay builddbm
	rm -f ../build ../debian/substvars ../debian/files

install:
	# Install the binaries
	$(DINSTALL) $(SBINDIR)
	$(DINSTALL) $(BINDIR)
	$(DINSTALL) $(RADIUS_DIR)
	$(DINSTALL) -m 700 $(DIRLOG)/radacct
#	$(INSTALL) -m 755 -s radutmp  $(DIRLOG)/radutmp
#	$(INSTALL) -m 755 -s radwtmp  $(DIRLOG)/radwtmp
#	$(INSTALL) -m 755 -s radius.log  $(DIRLOG)/radius.log
	$(INSTALL) -m 755 -s radiusd  $(SBINDIR)/radiusd
	$(INSTALL) -m 755 -s radrelay $(SBINDIR)/radrelay
	$(INSTALL) -m 755 -s radkill $(SBINDIR)/radkill
	$(INSTALL) -m 755 -s radwho  $(BINDIR)
	$(INSTALL) -m 755 -s raduse  $(BINDIR)
	$(INSTALL) -m 755 -s radzap  $(BINDIR)
	$(INSTALL) -m 755 -s radclient $(BINDIR)
	test -z "$(DBM)" || $(INSTALL) -m 755 -s builddbm $(BINDIR)
	$(INSTALL) -m 755    checkrad.pl $(SBINDIR)/checkrad
	$(INSTALL) -m 755    radlast $(BINDIR)
	$(INSTALL) -m 755    radtest $(BINDIR)
	# Install the man pages
	@for section in 1 5 8; do \
		$(DINSTALL) $(MANDIR)/man$$section; \
		for manfile in ../doc/*.$$section*; do \
		    echo $(INSTALL) -m 644 $$manfile $(MANDIR)/man$$section; \
		    $(INSTALL) -m 644 $$manfile $(MANDIR)/man$$section; \
		done ; \
	done
#	test -n "$(DBM)" || rm -f $(MANDIR)/man8/builddbm.8rad
#	@if [ -d /etc/pam.d/ ] && [ ! -f /etc/pam.d/radius ]; then \
#		$(INSTALL) -m 644 ../redhat/radiusd-pam /etc/pam.d/radius;\
#	fi
	# Install the config files
	@echo; echo "Creating/updating files in $(RADIUS_DIR)"; \
	cd ../raddb; \
	for i in [a-c]* [e-z]*; do \
		[ ! -f $(RADIUS_DIR)/$$i ] && cp $$i $(RADIUS_DIR)/$$i; \
	done; \
	for i in dictionary*; do \
		[ ! -f $(RADIUS_DIR)/$$i ] && cp $$i $(RADIUS_DIR)/$$i; \
		if [ "`find $$i -newer $(RADIUS_DIR)/$$i`" ]; then \
			echo "** $(RADIUS_DIR)/$$i"; \
			nt=1; \
		fi; \
	done; \
	if [ "$$nt" ]; then \
		echo "** The samples in ../raddb are newer than these files";\
		echo "** Please investigate and copy manually if appropriate";\
		echo ;\
	fi


