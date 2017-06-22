
Makefile documentation by Steve "Stevers!" Coile <scoile@patriot.net> 



BIN should identify the full specification of the directory in which
the command executables should be placed.


SBIN should identify the full specification of the directory in which
the system executables (daemons)should be placed.


CFLAGS should contain compiler command-line debug switches.  Typically,
the "-g" switch instructs the compiler to preserve symbol information
and suppresses optimization.  Setting CFLAGS to "-O2" maximizes
optimization and will make symbolic debugging impossible.  The "-g"
and "-O" switches are mutually exclusive.  "-O2" is probably a better
choice if you aren't actively working on server development.

CFLAGS can also include a number of macro's (-D<macroname>):

    -DNOSHADOW        Don't compile in shadow support (default for *BSD)
    -DNT_DOMAIN_HACK  Strip first part of NT_DOMAIN\loginname
    -DNOCASE          Dictionary file is case insensitive
    -DUSE_SYSLOG      Compile in syslog support

DBM should identify which DBM implementation your system provides as a
compiler command-line macro definition switch.
DBMLIB should identify the library containing the DBM implementation
as a linker command-line library specification switch.

Setting for several implementations:

   Library                      DBM=         DBMLIB=
-----------------------------+-------------+--------
Original (old) DBM library    -DUSE_DBM      -ldbm
BSD 4.3 NDBM library          -DUSE_NDBM     -lndbm
BSD 4.4 DB library (db1)      -DUSE_DB1      -ldb
Sleepycat DB library (db2)    -DUSE_DB2      -ldb		(*)
Sleepycat DB library (db3)    -DUSE_DB3      -ldb
Gnu DBM (gdbm)                -DUSE_GDBM     -lgdbm

(*) Correct setting for most Linux and *BSD systems


LIBS should identify the library containing the shadow password
support routines as a linker command-line library specification switch.
"-lshadow" is a typical value on systems that support shadow passwords
and don't have the shadow support routines integrated into libc or
another default library.  This parameter need only be defined if your
system supports shadow passwords.

