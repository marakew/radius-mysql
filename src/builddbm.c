/*
 * builddbm	Build DBM database from users file.
 *
 *		Copyright 2001 Cistron Internet Services B.V.
 */

char rcsid[] =
"$Id: builddbm.c,v 1.7 2001/11/10 12:41:38 miquels Exp $";

#include	<stdio.h>
#include	<unistd.h>
#include	<ctype.h>
#include	<errno.h>
#include	<sys/file.h>

#include	"radiusd.h"

#ifdef USE_DBM
#  include	<dbm.h>
#endif
#ifdef USE_NDBM
#  include	<ndbm.h>
#endif
#ifdef USE_GDBM
#  include	<gdbm-ndbm.h>
#endif
#ifdef USE_DB
#  include      <db.h>
#endif

char		*progname;
int		debug_flag;
int		recno = 1;

#ifdef DBIF_DB
#define dptr  data
#define dsize size
#define datum DBT
#endif

#define BUCKETS 16384
struct hashdata {
	char *key;
	int value;
	struct hashdata *next;
};
struct hashdata *buckets[BUCKETS];

void hash_stats(void)
{
	struct hashdata	*u;
	int		tot_used = 0;
	int		tot_len = 0;
	int		maxlen = 0;
	int		i, n;

	for (i = 0; i < BUCKETS; i++) {
		if ((u = buckets[i]) == NULL)
			continue;
		tot_used++;
		for (n = 0; u; u = u->next)
			n++;
		tot_len += n;
		if (n > maxlen) maxlen = n;
	}
	printf("Hash chain stats:\n");
	printf("Total buckets:     %d\n", BUCKETS);
	printf("Buckets used:      %d\n", tot_used);
	if (tot_used == 0) tot_used++;
	printf("Avg. chain length: %.1f\n", (float)tot_len / tot_used);
}

int hash_string(char *str)
{
	unsigned int hash = 0;
	while(*str)
		hash = 31 * hash + *str++;
	return (hash % BUCKETS);
}

/*
 *	Find username in hashtable.
 */
int hash_find(char *key)
{
	struct hashdata *u;

	u = buckets[hash_string(key)];
	while (u) {
		if (strcmp(u->key, key) == 0)
			return u->value;
		u = u->next;
	}

	return -1;
}

/*
 *	Store data in hash table.
 */
int hash_store(char *key, int value)
{
	struct hashdata *u;
	int hash = hash_string(key);

	u = buckets[hash];
	while (u) {
		if (strcmp(u->key, key) == 0)
			break;
		u = u->next;
	}
	if (u) {
		u->value = value;
		return 0;
	}
	if ((u = malloc(sizeof(struct hashdata))) == NULL ||
	    (u->key = strdup(key)) == NULL) {
		perror("malloc");
		exit(1);
	}
	u->value = value;
	u->next = buckets[hash];
	buckets[hash] = u;

	return 0;
}

/*
 *	Fill buffer with attribute/value pairs.
 */
int fill2(char *buf, int bufsz, VALUE_PAIR *pairs)
{
	VALUE_PAIR	*vp;
	char		*ptr, *s;
	char		*quote, *comma;
	char		tag[32];
	int		l, left, done;

	ptr = buf;
	left = bufsz;
	done = 0;

	for (vp = pairs; vp != NULL; vp = vp->next) {
		l = strlen(vp->name) + 3;
		quote = "";
		for (s = vp->strvalue; *s; s++) {
			if (strchr(" \t,=", *s))
				quote = "\"";
			l++;
		}
		if (quote[0]) l += 2;
		comma = vp->next ? "," : "";
		if (comma[0]) l++;
		tag[0] = 0;
		if (vp->flags.has_tag) {
#ifdef MERIT_TAG_FORMAT
			sprintf(tag, ":%d:", vp->flags.tag);
#else
			sprintf(tag, ":%d", vp->flags.tag);
#endif
			l += strlen(tag);
		}
		if (l >= left - 1)
			return bufsz;
#ifdef MERIT_TAG_FORMAT
		sprintf(ptr, "%s = %s%s%s%s%s",
#else
		sprintf(ptr, "%s%s = %s%s%s%s",
#endif
			vp->name, tag, quote, vp->strvalue, quote, comma);
		ptr  += l;
		done += l;
		left -= l;
	}
	ptr[0] = 0;

	return done;
}

void fill(char *buf, int bufsz, char *name, int recno, VALUE_PAIR *check,
		VALUE_PAIR *reply)
{
	int		n;
	char		*m;

	m = "builddbm: entry for %s too large - truncated\n";

	sprintf(buf, "%d\n", recno);
	n = strlen(buf);
	buf += n;
	bufsz -= n;

	if ((n = fill2(buf, bufsz, check)) == bufsz || n > bufsz - 2) {
		fprintf(stderr, m, name);
		return;
	}

	buf += n;
	bufsz -= n;
	strcpy(buf, "\n");
	buf++;
	bufsz--;

	if ((n = fill2(buf, bufsz, reply)) == bufsz) {
		fprintf(stderr, m, name);
		return;
	}
}

static int users(char *file, void *dbm)
{
	FILE		*fp;
	VALUE_PAIR	*check, *reply;
	datum		key;
	datum		value;
	int		lineno = 0;
	int		l, num;
	char		content[1024];
	char		buffer[256];
	char		name[512];
	char		*ptr, *s;

	if ((fp = fopen(file, "r")) == NULL) {
		fprintf(stderr, "builddbm: ");
		perror(file);
		return -1;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		lineno++;

		if (buffer[0] == '#' || buffer[0] == '\n' ||
		    buffer[0] == '\r') continue;

		/*
		 *	Include another file if we see
		 *	$INCLUDE filename
		 */
		if (strncasecmp(buffer, "$include", 8) == 0) {
			ptr = buffer + 8;
			while(isspace(*ptr))
				ptr++;
			s = ptr;
			while (!isspace(*ptr))
			ptr++;
			*ptr = 0;

			if (users(s, dbm) < 0) {
				fclose(fp);
				return -1;
			}
			continue;
		}

		/*
		 *	Process one entry.
		 */
		if (read_entry(buffer, fp, file, &lineno, name,
		    &check, &reply, 0) < 0) {
			fclose(fp);
			return -1;
		}
		auth_type_fixup(check);

		/*
		 *	And store it.
		 */
		fill(content, sizeof(content), name, recno++, check, reply);
		pairfree(check);
		pairfree(reply);

		/* Add \nnum to name */
		if ((num = hash_find(name)) >= 0) {
			num++;
			hash_store(name, num);
			l = strlen(name);
			sprintf(name + l, "\n%d", num);
		} else {
			hash_store(name, 0);
		}

		memset(&key, 0, sizeof(key));
		key.dptr = name;
		key.dsize = strlen(name);
		memset(&value, 0, sizeof(value));
		value.dptr = content;
		value.dsize = strlen(content);

#ifdef DBIF_DBM
		if (store(key, value) != 0)
#endif
#ifdef DBIF_NDBM
		if (dbm_store(dbm, key, value, DBM_INSERT) != 0)
#endif
#ifdef DBIF_DB1
		if (((DB *)dbm)->put(dbm, &key, &value, 0) != 0)
#endif
#if defined(DBIF_DB2) || defined(DBIF_DB3)
		if ((l = ((DB *)dbm)->put(dbm, NULL, &key, &value, 0)) != 0)
#endif
		{
#ifdef DBIF_DB3
			fprintf(stderr, "%s: %s\n", progname,
				db_strerror(l));
#endif
			fprintf(stderr, "%s: Couldn't store entry for \"%s\"\n",
				progname, name);
			exit(1);
		}
	}

	return 0;
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-d config_dir]\n",
		progname);
	exit(1);
}

int main(int argc, char **argv)
{
#ifdef DBIF_DBM
	int		fd;
#endif
#ifdef DBIF_NDBM
	DBM		*dbm;
#endif
#ifdef DBIF_DB
	DB		*dbm;
#endif
	int		r, c;
	char		*radius_dir = RADIUS_DIR;
	int		verbose = 0;

	progname = "builddbm";
	while ((c = getopt(argc, argv, "d:v")) != EOF) switch(c) {
		case 'd':
			radius_dir = optarg;
			if (chdir(optarg) < 0) {
				fprintf(stderr, "%s: ", progname);
				perror(optarg);
				exit(1);
			}
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			break;
	}

	dict_init(radius_dir, NULL);

	/*
	 *	Initialize a new, empty database.
	 */
#if !defined(USE_DB)
	(void)unlink("users~.dir");
	(void)unlink("users~.pag");
#endif
	(void)unlink("users~.db");
#ifdef DBIF_DBM
	if ((fd = open("users~.pag", O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
		fprintf(stderr, "%s: Couldn't open users~.pag for writing\n",
			progname);
		exit(1);
	}
	close(fd);
	if ((fd = open("users~.dir", O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0) {
		fprintf(stderr, "%s: Couldn't open users~.dir for writing\n",
			progname);
		exit(1);
	}
	close(fd);
	if (dbminit("users~") != 0) {
		fprintf(stderr, "%s: ", progname);
		perror("dbminit(users~)");
		exit(1);
	}
#endif
#ifdef DBIF_NDBM
	if ((dbm = dbm_open("users~", O_RDWR|O_CREAT, 0600)) == NULL) {
		fprintf(stderr, "%s: ", progname);
		perror("dbm_open(users~)");
		exit(1);
	}
#endif
#ifdef DBIF_DB1
	if ((dbm = dbopen("users~.db", O_RDWR|O_CREAT, 0600,
	     DB_HASH, NULL)) == NULL) {
		fprintf(stderr, "%s: ", progname);
		perror("dbopen(users~)");
		exit(1);
	}
#endif
#ifdef DBIF_DB2
	if (db_open("users~.db", DB_HASH, DB_CREATE, 0600, NULL, NULL, &dbm)<0) {
		fprintf(stderr, "%s: ", progname);
		perror("db_open(users~)");
		exit(1);
	}
#endif
#ifdef DBIF_DB3
	if (db_create(&dbm, NULL, 0) < 0 ||
	    dbm->open(dbm, "users~.db", NULL, DB_HASH, DB_CREATE, 0600) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror("db->open(users~)");
		exit(1);
	}
#endif

#ifdef DBIF_DBM
	r = users("users", NULL);
#else
	r = users("users", dbm);
#endif

#ifdef DBIF_DBM
	dbmclose();
#endif
#ifdef DBIF_NDBM
	dbm_close(dbm);
#endif
#ifdef DBIF_DB1
	dbm->close(dbm);
#endif
#if defined (DBIF_DB2) || defined(DBIF_DB3)
	dbm->close(dbm, 0);
#endif

	/* Now rename */
#if !defined(USE_DB)
	(void)rename("users~.dir", "users.dir");
	(void)rename("users~.pag", "users.pag");
#endif
	(void)rename("users~.db", "users.db");

	if (verbose) hash_stats();

	return r == 0 ? 0 : 1;
}

