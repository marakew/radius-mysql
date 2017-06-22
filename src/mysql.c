#ifdef USEMYSQL

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <strings.h>

#include <mysql/mysql.h>
#include <mysql/mysql_version.h>
#include "radiusd.h"


u_int		holiday_table[40];

char		sql_server[20];
char		sql_login[20];
char		sql_password[20];
char		sql_dbase[20];
MYSQL		real_mysql, *mysql = NULL;
char		auth_table[20];
char		acct_table[20];

/* ===================================================================
 *		status: done read config! init other param ?:-/
 */

int	sql_read_config() {
	
	MYSQL_RES	*res;
	MYSQL_ROW	row;
	FILE		*sqlfd;
	char		dummystr[64];
	char		namestr[64];
	int		line_no = 0;
	char		buffer[256];
	char		sqlfile[256];
	char		buff_query[100];
	int		ihour = 0;
//	double		ctest = 0;

	strcpy(sql_server,"");
	strcpy(sql_login,"");
	strcpy(sql_password,"");
	strcpy(sql_dbase,"");
	strcpy(auth_table,"userdata");
	strcpy(acct_table,"usertime");
	hourgroup = 0;
	trafgroup = 0;
	acct_alive = 0;
	passwdhash = 0;
	append_table = -1;
	downcost_type = 0;

	sprintf(sqlfile,"%s/%s",radius_dir,"radiusd.conf");
	
 	if ((sqlfd = fopen(sqlfile,"r")) == NULL ){
		log(L_ERR,"could not read file %s",sqlfile);
		return -1;
	 }

	while(fgets(buffer,sizeof(buffer),sqlfd) != NULL){
		line_no++;

	if(*buffer == '#' || *buffer == '\0' || *buffer == '\n')
		continue;

	if (strncasecmp(buffer, "server", 6) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else 	strcpy(sql_server,namestr);	
	}

	if (strncasecmp(buffer, "login", 5) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else	strcpy(sql_login,namestr);
	}

	if (strncasecmp(buffer, "password", 8) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else	strcpy(sql_password,namestr);
	}

	if (strncasecmp(buffer, "auth_db", 7) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else	strcpy(sql_dbase,namestr);
	}

	if (strncasecmp(buffer, "auth_table", 10) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else	strcpy(auth_table,namestr);
	}

	if (strncasecmp(buffer, "acct_table", 10) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else	strcpy(acct_table,namestr);
	}

	if (strncasecmp(buffer, "hourgroup", 9) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else{
			if (strncasecmp(namestr, "yes", 3) == 0)
				hourgroup = 1;
			else	hourgroup = 0;
		  }
	}
	
	if (strncasecmp(buffer, "trafgroup", 9) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else{
			if (strncasecmp(namestr, "yes", 3) == 0)
				trafgroup = 1;
			else	trafgroup = 0;
		  }
	}

	if (strncasecmp(buffer, "passwdhash", 10) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else{
			if (strncasecmp(namestr, "yes", 3) == 0)
				passwdhash = 1;
			else	passwdhash = 0;
		  }
	}

	if (strncasecmp(buffer, "acct_alive", 10) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else{
			acct_alive = atoi(namestr);
			if(acct_alive &&
			   acct_alive < 60){
			   acct_alive = 60;
			}
		  }
	}

#ifdef CREATE_NAME_TABLE
	if (strncasecmp(buffer, "append_table", 12) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else{
			append_table = atoi(namestr);
			if(append_table &&
			   append_table < 60){
			   append_table = 60;
			}
		  }
	}
#endif

	if (strncasecmp(buffer, "downcost", 8) == 0) {
	 if (sscanf(buffer,"%s%s", dummystr, namestr) != 2) {
		log(L_ERR,"invalid attribute on line %d of file %s",
			line_no,sqlfile);
		}else{
			if (strncasecmp(namestr, "lineral", 7) == 0)
				downcost_type = 0;
			else	downcost_type = 1;
		  }
	}

	} /* while fget */
	
	fclose(sqlfd);

 	if (!(mysql = sql_reconnect())) return -1;

		/* ------------------------------
		 * Group select 
		 */

		sprintf(buff_query,
		"SELECT gid,grpname,traf_cost,1mb,type,"
		"daycost,use_callback,type_traf,prefix "
		"FROM grp ORDER BY gid");

	if (!sql_query(buff_query,mysql))
	 if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR,"MYSQL error: %s",mysql_error(mysql));
	 } else {
	 u_int igroup = 0;
		while((row = mysql_fetch_row(res))){
			group[igroup].gid = atoi(row[0]);
			strcpy(group[igroup].grname,row[1]);
			group[igroup].traf_cost = atof(row[2]);
			group[igroup].one_mb = atoi(row[3]);
			group[igroup].type = atoi(row[4]);
			group[igroup].freesec = 0;
			group[igroup].day_cost = atof(row[5]);
			group[igroup].use_callback = atoi(row[6]);
			group[igroup].type_traf = atoi(row[7]);
			group[igroup].prefix = row[8][0];
#if 1
	DEBUG("(gid:%d,grname:%12s,trafcost:%.2f,1mb:%d,type:%d,daycost:%.2f,callback:%d,typetraf:%d,prefix:%c)",
			group[igroup].gid,
			group[igroup].grname,
			group[igroup].traf_cost,
			group[igroup].one_mb,
			group[igroup].type,
			group[igroup].day_cost,
			group[igroup].use_callback,
			group[igroup].type_traf,
			group[igroup].prefix);
#endif

		if (group[igroup].type > 4){
		log(L_ERR,"Group ( %s [%d] ) Type - [ %d ] incorrect.",
			group[igroup].grname,
			group[igroup].gid,
			group[igroup].type);
			continue;
		}
		
		if (group[igroup].gid == 0){
		  log(L_ERR,"Group ( %s ) GID is NULL", group[igroup].grname);
			continue;
			}
		igroup++;
		
		if (igroup > MAX_NUM_GROUP){
		  log(L_ERR,"LIMIT %d group", MAX_NUM_GROUP);
		  break;
		}

		} /* while */
	group[igroup].grname[0] = 0;
	group[igroup].type = 0;

	mysql_free_result(res); 
	}

	log(L_ERR,"group read done");

	if (hourgroup == 1 || trafgroup == 1){

		/* --------------------------------
		 *  Hourgroup select
		 */
		ihour = 0;
	if (group[ihour].gid && hourgroup == 1){
	 do {
		ihour++;
		if (ihour > MAX_NUM_GROUP){
		  log(L_ERR,"LIMIT %d group", MAX_NUM_GROUP);
		  break;
		}
		if (group[ihour].type != 1 &&
		    group[ihour].type != 2 )continue;
	sprintf(buff_query,
			"SELECT * FROM hourgroup WHERE gid=%d",
			group[ihour].gid);
	if (!sql_query(buff_query,mysql))
	  if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR,"MYSQL error: %s",mysql_error(mysql));
		mysql_free_result(res); 
	  } else {
		if (mysql_num_rows(res) != 8){
		log(L_ERR,"Hourgroup ( %s [%d] ) incorrect.",
				group[ihour].grname,
				group[ihour].gid);
		}else{
			while((row = mysql_fetch_row(res))){
			int i = 0;
			int weeks = atoi(row[24]);
				do{
				 group[ihour].hour[weeks][i] = atof(row[i]);
				}while(++i < 24);
		     }
	/*	mysql_free_result(res); */
		}
		mysql_free_result(res); 
	 }
#if 0
		ihour++;
		if (ihour > MAX_NUM_GROUP){
		  log(L_ERR,"LIMIT %d group", MAX_NUM_GROUP);
		  break;
		}
#endif
	   } while (group[ihour].gid != 0);
	}	

	log(L_ERR,"hourgroup read done");

		/* --------------------------------
		 *  Trafgroup select
		 */
		ihour = 0;
	if (group[ihour].gid && trafgroup == 1){
	 do {
		ihour++;
		if (ihour > MAX_NUM_GROUP){
		  log(L_ERR,"LIMIT %d group", MAX_NUM_GROUP);
		  break;
		}
		if (group[ihour].type != 2 &&
		    group[ihour].type != 3 )continue;
	sprintf(buff_query,
			"SELECT * FROM trafgroup WHERE gid=%d",
			group[ihour].gid);
	if (!sql_query(buff_query,mysql))
	  if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR,"MYSQL error: %s",mysql_error(mysql));
		mysql_free_result(res); 
	  } else {
		if (mysql_num_rows(res) != 8){
		log(L_ERR,"Trafgroup ( %s [%d] ) incorrect! use group.traf_cost",
				group[ihour].grname,
				group[ihour].gid);
		} else {
			group[ihour].traf_cost = -1; /* we are use trafgroup */
			while((row = mysql_fetch_row(res))){
			int i = 0;
			int weeks = atoi(row[24]);
				do{
				 group[ihour].traf[weeks][i] = atof(row[i]);
				} while(++i < 24);
		     }
		/*mysql_free_result(res);*/
		}
		mysql_free_result(res);
	 }
#if 0
		ihour++;
		if (ihour > MAX_NUM_GROUP){
		  log(L_ERR,"LIMIT %d group", MAX_NUM_GROUP);
		  break;
		}
#endif
	   } while(group[ihour].gid != 0);
	}	

	log(L_ERR,"trafgroup read done");

		/*------------------------------
		 *   Holiday
		 */

		sprintf(buff_query,
     "SELECT dayofyear(concat(year(now()),'-',hl)) FROM holiday ORDER BY hl");
	

	if (!sql_query(buff_query,mysql))
	 if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR,"MYSQL error: %s",mysql_error(mysql));
	 } else {
		int i = 0;
		while (row = mysql_fetch_row(res)){
		 if (row[0] == NULL){
			log(L_ERR,"Holiday date incorrect.");
		  }else{
			holiday_table[i] = atoi(row[0]);
			i++;
		  }
		    
		}
	 holiday_table[i] = 0;
	 mysql_free_result(res);
	 } 

	} // hourgroup && trafgroup

	log(L_ERR,"trafgroup & hourgroup read done");
		/*------------------------------
		 *   Tariff
		 */
		ihour = 0;
	if (group[ihour].gid){
	 do {
		ihour++;
		if (ihour > MAX_NUM_GROUP){
		  log(L_ERR,"LIMIT %d group", MAX_NUM_GROUP);
		  break;
		}
		if (group[ihour].type != 2 &&
		    group[ihour].type != 3 )continue;
	sprintf(buff_query,
			"SELECT type,bytes,cost FROM tariff WHERE gid=%d"
			" ORDER BY bytes ASC",
			group[ihour].gid);
	if (!sql_query(buff_query,mysql))
	  if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR,"MYSQL error: %s",mysql_error(mysql));
		mysql_free_result(res); 
	  } else {
		u_int i = 0;
		while((row = mysql_fetch_row(res))){
				 group[ihour].traf_s[i].type = atoi(row[0]);
				//kir++
				 group[ihour].traf_s[i].bytes = atol(row[1]);
				 group[ihour].traf_s[i].cost = atof(row[2]);
			//kir++
			 group[ihour].traf_s[i].bytes *= group[ihour].one_mb;

			DEBUG("tariff[%d]: type=%d,bytes=%12ld,cost=%f",
				i,
				group[ihour].traf_s[i].type,
				group[ihour].traf_s[i].bytes,
				group[ihour].traf_s[i].cost);
			i++;
		if (i > 10){
		  log(L_ERR,"LIMIT 10 tariff's per group");
		  break;
		}
		} /* while */
		mysql_free_result(res); 
	 }
#if 0
		ihour++;
		if (ihour > MAX_NUM_GROUP){
		  log(L_ERR,"LIMIT %d group", MAX_NUM_GROUP);
		  break;
		}
#endif
	   } while (group[ihour].gid != 0);
	}
	log(L_ERR,"tariff read done");


//	ctest = time2moneyH(1049907109,1049914716,0);
//	ctest = time2moneyH(1049831487,1049832516,0);
//	ctest = time2moneyH(1050088496,1050088693,0);
//	ctest = time2moneyH(1050168995,1050169720,0);
//	ctest = time2moneyH(1050580771,1050580788,0);
//	ctest = time2moneyH(1054303030,1054303074,0);
//	DEBUG("%f",ctest);
//	line_no = money2time(0.000003,0);
//	DEBUG("%ld",line_no);

 	if (mysql)
		mysql_close(mysql);	

	mysql = NULL;

	log(L_INFO,
		"MYSQL_init: using: %s,%s,%s,%s,%s,"
		"hourgroup:%s,trafgroup:%s,passwdhash:%s,acct_alive(%s):%d sec",
		sql_server,
		sql_login,
		sql_dbase,
		auth_table,
		acct_table,
		hourgroup ? "yes":"no",
		trafgroup ? "yes":"no",
		passwdhash ? "yes":"no",
		acct_alive ? "yes":"no", acct_alive);

	return 0;
}

/*=====================================================================
 *		status: done  - may be extended
 */

void	get_session(VALUE_PAIR *pair, SESSION_REC *rec){

	char	buffer[20];
	char	*str;

	switch (pair->attribute) {

		case PW_ACCT_SESSION_ID:
			strncpy(rec->id,pair->strvalue,32);
			break;

		case PW_USER_NAME:
		if((u_char)(pair->strvalue[0]-('A')) <= (u_char)('Z'-'A')){
				strcpy(rec->name,pair->strvalue+1);
				rec->prefix = pair->strvalue[0];
			}else{
				strcpy(rec->name,pair->strvalue);
				rec->prefix = ' ';
			}
			break;

		case PW_ACCT_STATUS_TYPE:
			rec->start = pair->lvalue;
			break;

		case PW_NAS_IP_ADDRESS:
			ipaddr2str(buffer,pair->lvalue);
			strcpy(rec->server,buffer);
			break;

		case PW_CLIENT_IP_ADDRESS:
			/* some time fild NAS-IP-Address empty
			 * if it's true, we use Client-IP-Address as one
			 */
			if (strlen(rec->server) == 0){
				ipaddr2str(buffer,pair->lvalue);
				strcpy(rec->server,buffer);
#if 0
				log(L_DBG, "NAS-IP-Address empty"
				" - use Client-IP-Address %s",rec->server);
#endif
			}
			break;

		case PW_FRAMED_IP_ADDRESS:
			ipaddr2str(buffer,pair->lvalue);
			strcpy(rec->ip,buffer);
			break;

		case PW_NAS_PORT:
			rec->port = pair->lvalue;
			break;

		case PW_SERVICE_TYPE:
			rec->service_type = pair->lvalue;
			break;

		case PW_FRAMED_PROTOCOL:
			rec->protocol = pair->lvalue;
			break;

		case PW_LOGIN_SERVICE:
			rec->protocol = pair->lvalue+100;
			break;

		case PW_ACCT_SESSION_TIME:
			rec->time_used = pair->lvalue;
			break;

		case PW_ACCT_AUTHENTIC:
			rec->auth = pair->lvalue;
			break;

		case PW_ACCT_OUTPUT_OCTETS:
#ifdef NORMAL_ACCT
			rec->outbytes = pair->lvalue;
#else
			rec->inbytes = pair->lvalue;
#endif
			break;

		case PW_ACCT_INPUT_OCTETS:
#ifdef NORMAL_ACCT
			rec->inbytes = pair->lvalue;
#else
			rec->outbytes = pair->lvalue;
#endif
			break;

		case PW_LOGIN_IP_HOST:
			ipaddr2str(buffer,pair->lvalue);
			strcpy(rec->ip,buffer);
			break;

		case PW_FRAMED_COMPRESSION:
			break;

		case PW_ACCT_INPUT_PACKETS:
#ifdef NORMAL_ACCT
			rec->inpackets = pair->lvalue;
#else
			rec->outpackets = pair->lvalue;
#endif
			break;

		case PW_ACCT_OUTPUT_PACKETS:
#ifdef NORMAL_ACCT
			rec->outpackets = pair->lvalue;
#else
			rec->inpackets = pair->lvalue;
#endif
			break;

		case PW_ACCT_DELAY_TIME:
			rec->delay = pair->lvalue;
			break;

		case PW_NAS_PORT_TYPE:
			rec->port_type = pair->lvalue;
			break;

		case PW_CONNECT_INFO:
			strcpy(rec->connect_info,pair->strvalue);
			break;

		default:

			if (!strcasecmp(pair->name,"called-station-id")){
				strcpy(rec->call_to,pair->strvalue);
			} else
			if (!strcasecmp(pair->name,"calling-station-id")){
				int count = 0;
				int i = 0;
				str = pair->strvalue;
			     while ((*str != '\0') && (i < 3)) {
				if ((*str >= '\200') && (*str <= '\205'))
					count++;
				str++; i++;
			    }
				strcpy(rec->call_from, pair->strvalue + count);
			} else
			if (!strcasecmp(pair->name, "Acct-Terminate-Case")){
				 rec->term_cause = pair->lvalue; 
			} else {
#if 0
				log(L_INFO,"Unknown Pair: %s", pair->name);
#endif
			}
	}
}

/*========================================================================
 *		status: done
 */

MYSQL	*sql_reconnect(){
	
	int	   i = 0;
	MYSQL *mysql = NULL;

#if MYSQL_VERSION_ID >= 40013 
	mysql_init(&real_mysql);
	for (i = 0; i < 6; i++)
	  if (!(mysql_real_connect(&real_mysql,
				sql_server,
				sql_login,
				sql_password,
				sql_dbase, 0, NULL, CLIENT_FOUND_ROWS))){
		log(L_ERR,"MYSQL: Cannot Connect to %s@%s:%s",
			sql_server, sql_login, sql_dbase);
		mysql = NULL;
		sleep(5);
	  }else{
		mysql = &real_mysql;
		break;
	  }

#else
	for (i = 0; i < 6; i++)
	  if (!(mysql_connect(&real_mysql,sql_server,sql_login,sql_password))){
		log(L_ERR,"MYSQL: Cannot Connect to %s as %s",
			sql_server, sql_login);
		mysql = NULL;
		sleep(5);
	  }else{
		mysql = &real_mysql;
		break;
	  }
	  if (mysql != NULL)
		for (i = 0; i < 6; i++)
		  if (mysql_select_db(mysql,sql_dbase)){
			log(L_ERR,"MYSQL cannot select db %s", sql_dbase);
			log(L_ERR,"MYSQL error: %s", mysql_error(mysql));
			sleep(5);
		  }else{
#if 0
			log(L_INFO,"MYSQL Connected to db %s", sql_dbase); 
#endif
			return mysql;
		   }
#endif
		log(L_ERR, "MYSQL: Giving up on connect");
		return mysql;
}

/*===================================================================
 *	run query
 *		status: done!
 */

int	sql_query(const char *query, MYSQL *mysql){
	
	int	r = 0;
	int	i = 0;
	while( i < 10){
//		if (mysql == NULL)
//			mysql = sql_reconnect();
		if (mysql == NULL && !(mysql = sql_reconnect()))
			return 0;
#if 1
		DEBUG("%s", query);
#endif
		r = mysql_query(mysql,query);

	  if (r == NULL){
#if 0
		DEBUG("MYSQL query OK");
#endif
		return r;
	  }
	  if (!strcasecmp(mysql_error(mysql),"MySQL server has gone away")){
		log(L_ERR,"MYSQL Error (retrying %d): Cannot Query:%s",i,query);
		log(L_ERR,"MYSQL error: %s", mysql_error(mysql));
		mysql_close(mysql);
		mysql = NULL;
	   }else{
		log(L_ERR,"MYSQL Error (%d): Cannot Query:%s", r, query);
		log(L_ERR,"MYSQL error: %s", mysql_error(mysql));
		return r;
	  }
		i++;
	}
		log(L_ERR,"MYSQL Error (giving up): Cannot Query:%s", query);
	return r;
}


/* ===================================================================
 *		status: done! 
 */

int	sql_killuser(SESSION_REC *rec){

	char	buf[4096];
	char	*argv[32];
	char	*p;
	pid_t	pid;
	int	n,argc = -1;

	sprintf(buf, "/usr/local/sbin/radkill %s %d %s", 
				rec->server, rec->port, rec->ip);

	if ((pid = fork()) == 0){
		log(L_INFO, "Kill-Program: %s", buf);

		p = strtok(buf, " \t");
		if (p) do {
			argv[++argc] = p;
			p = strtok(NULL, " \t");
		} while(p != NULL);
		argv[++argc] = p;

		if (argc == 0) {
			log(L_ERR, "Kill-Program: empty command line.");
			exit(1);
		}

		for (n = 32; n >= 3; n--)
			close(n);
		
		execvp(argv[0], argv);
	
		log(L_ERR, "Kill-Program: %s: %m", argv[0]);
		exit(1);
	}

	if (pid < 0){
		log(L_ERR, "Couldn't fork: %m");
		return -1;
	}

	return 0;
}

/* ===================================================================
 *		status: working!
 */

void	save_session(SESSION_REC *rec){

	MYSQL_RES	*res;
	MYSQL_ROW	row;
	SESSION_REC	fix_rec;

	char		buff_query[256*2];
	char		datebuf[100];
	struct	tm	*time_now;
	time_t		curtime;
 	long		time_on, start_time, stop_time;
	long		sinbytes, soutbytes;
	long		inbytes, outbytes;
	long		limit_timeon = 0, limit_inbytes = 0, limit_outbytes = 0;
	u_int		nid = 0;
	u_int		status;
	u_int		igroup = 0;
	double		deposit = 0, credit;
	double		dcost = 0, scosth = 0, scostt = 0;
	double		ccostH = 0, ccostT = 0, cost = 0;
	double		ccosth = 0, ccostt = 0;
	u_int 		gid = 0;

	if (mysql == NULL)
		mysql = sql_reconnect();
	
//	curtime = time(0) - rec->delay;
	time(&curtime);
	curtime -= rec->delay;

	time_now = localtime(&curtime);
//	localtime_r(&curtime, time_now);

	strftime(datebuf, sizeof(datebuf), "%Y%m%d%H%M%S", time_now);

	if (rec->start == PW_STATUS_ACCOUNTING_ON ||
	    rec->start == PW_STATUS_ACCOUNTING_OFF ){

	/* The Terminal server informed us that it was rebooted */
	/* STOP all records from this NAS */
		log(L_INFO, "NAS '%s' rebooted (%s) at %s",
			 rec->server, 
			 rec->start == PW_STATUS_ACCOUNTING_ON? "On":
			 rec->start == PW_STATUS_ACCOUNTING_OFF? "Off": "??",
			datebuf);
#ifdef NEW_FIX
		sprintf(buff_query,
		"UPDATE %s SET stop_time='%s',"
		"time_on=unix_timestamp('%s')-unix_timestamp(start_time),"
		"term_cause=%d "
		"WHERE server='%s' AND start_time<='%s'",
		acct_table,
	 	datebuf,
		datebuf,
		rec->term_cause,
		rec->server,
		datebuf);	

		 DEBUG("%s", buff_query);
#if 0
	     if (sql_query(buff_query,mysql))
		 log(L_ERR,"MYSQL Error: Cannot Update");	
	     else
		 log(L_INFO, "UPDATE %ld rows", (long) mysql_affected_rows(mysql));
#endif

		return;		/*	FIX ME !!!! */
#else

		/* continue down */

		sprintf(buff_query,
		"SELECT id,server,name,unix_timestamp('%s')-unix_timestamp(start_time)"
		" FROM %s WHERE server='%s' AND start_time<='%s' "
		"AND (status = %d OR status = %d)",
		datebuf,
		acct_table,
		rec->server,
		datebuf,
		PW_STATUS_START,
		PW_STATUS_ALIVE);

	if (!sql_query(buff_query,mysql))
	  if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR,"MYSQL error: %s",mysql_error(mysql));
		mysql_free_result(res); 
	  } else {

		while (row = mysql_fetch_row(res)){

		bzero(&fix_rec,sizeof(fix_rec));

		fix_rec.start = PW_STATUS_STOP;
		strncpy(fix_rec.id, row[0], 32);
		strcpy(fix_rec.server, row[1]);
		strcpy(fix_rec.name, row[2]);
		fix_rec.time_used = atol(row[3]); 
/*		strcpy(fix_rec.stop_time, datebuf); */

		DEBUG("id:[%s] server:[%s] name:[%s] time_used:[%ld]", 
		fix_rec.id, fix_rec.server, fix_rec.name, fix_rec.time_used); 

		/* we need to ZAP! for Simultaneous-Use */
		radzap(rec->server, rec->port, rec->name, curtime, 1);

		save_session(&fix_rec);	  /* recursive FIX for ALL USER */	
		  }

	  mysql_free_result(res);
	  }

		return;

#endif

	}

		sprintf(buff_query,
			"SELECT id,time_on,status,inbytes,outbytes,"
			"costh,costt FROM %s "
			"WHERE id='%s' AND server='%s' AND name='%s'",
			acct_table,
			rec->id,
			rec->server,
			rec->name);

	if(!sql_query(buff_query,mysql))
	   if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR, "MYSQL error: %s", mysql_error(mysql));
	   }else{
		if ((nid = mysql_num_rows(res)) == 1){
			row = mysql_fetch_row(res);
			time_on = atol(row[1]);	
			status = atoi(row[2]);
			sinbytes = atol(row[3]);
			soutbytes = atol(row[4]);
			scosth = atof(row[5]);
			scostt = atof(row[6]);

			DEBUG("have SESSION time_on %d status %d"
				" inbytes=%d outbytes=%d costh=%f costt=%f",
						 time_on, status,
				  sinbytes, soutbytes, scosth, scostt);
		} else {
		/*	row = mysql_fetch_row(res); */
			time_on = 0;
			/*status = PW_STATUS_START;*/
			status = rec->start;
			sinbytes = 0;
			soutbytes = 0;
			scosth = 0;
			scostt = 0;

			DEBUG("no SESSION time_on %d status %d - FIX", 
						time_on, status);
		}
		mysql_free_result(res);
	   }


#if 1
	/* drop STOP packet after ALIVE (dub) if user reach limit and has kill 
	   drop ALIVE packet after STOP (session already close)	*/

	/* if session exist and have status STOP!
	 * but after some secs arrived packet has STATUS ALIVE
	 * drop and do nothig !
	 */
	if ((rec->start == PW_STATUS_ALIVE && 
	     status == PW_STATUS_STOP) &&
	     rec->time_used <= (time_on + 5) &&
	     nid == 1){
		log(L_INFO, "NAS %s [user %s] drop %s(%d) [status %s(%d)]"
			"packet at {time_used %d == time_on %d} sec", 
			rec->server, rec->name,
			(rec->start == PW_STATUS_STOP)? "Stop":"Alive", 
			rec->start, 
			(status == PW_STATUS_STOP)? "Stop":"Alive",
			status,
			rec->time_used, time_on);

		/* we need to ZAP! for Simultaneous-Use */
		radzap(rec->server, rec->port, rec->name, curtime, 1);
		log(L_INFO, "radzap NAS %s %d [user %s]",
				rec->server, rec->port, rec->name);
		return;
	    }
#endif


#ifdef CREATE_NAME_TABLE

	if (append_table > 60){
		DEBUG("time_used %d, time_on %d", rec->time_used, time_on);
		DEBUG("time_used %d%, time_on %d%", 
			rec->time_used % append_table, time_on % append_table);

	if ( (rec->time_used == 0) || 
		((rec->time_used % append_table) <= (time_on % append_table)) ){
 
		sprintf(buff_query,
		"INSERT INTO %s "
		"(id,time_on,inbytes,outbytes,start_time)"
		" VALUES "
		"('%s',%ld,%u,%u,'%s')",
		rec->name,
		rec->id,
		rec->time_used,
		rec->inbytes,
		rec->outbytes,
		datebuf);

		DEBUG("%s", buff_query);

	     if (sql_query(buff_query,mysql))
		 log(L_ERR,"MYSQL Error: Cannot insert");
	}
	}
#endif

	if (mysql != NULL && nid == 0 &&
		(rec->start == PW_STATUS_START ||
		 rec->start == PW_STATUS_STOP  ||
		 rec->start == PW_STATUS_ALIVE) ){

		sprintf(buff_query,
		"INSERT INTO %s "
		"(id,time_on,name,server,port,ip,inbytes,outbytes,"
		"cost,call_to,call_from,term_cause,prefix )"
		" VALUES "
		"('%s',%ld,'%s','%s',%i,'%s',%u,%u,0,'%s','%s',%d,'%c')",
		acct_table,
		rec->id,
		rec->time_used,
		rec->name,
		rec->server,
		rec->port,
		rec->ip,
		rec->inbytes,
		rec->outbytes,
		rec->call_to,
		rec->call_from,
		rec->term_cause,
		rec->prefix);

	     if (sql_query(buff_query,mysql))
		 log(L_ERR,"MYSQL Error: Cannot insert");	
	 }

	if (rec->xmit_rate || rec->data_rate ){
	/* sprintf(," [%d/%d], rec->xmit_rate, rec->data_rate); */
	}

	if (rec->start == PW_STATUS_START ){

		sprintf(buff_query,
		"UPDATE %s SET start_time=%s,connect_info='%s',status=%d "
		"WHERE id='%s' AND server='%s' AND name='%s'",
		acct_table,
		datebuf,
		rec->connect_info,
		rec->start,
		rec->id,
		rec->server,
		rec->name);
	} else
	if (rec->start == PW_STATUS_STOP || 
	    rec->start == PW_STATUS_ALIVE ){


	if (rec->start == PW_STATUS_ALIVE ){
		sprintf(buff_query,
		"UPDATE %s SET time_on=%lu,stop_time='%s',"
		"inbytes=%u,outbytes=%u,term_cause=%d,status=%d "
		"WHERE id='%s' AND server='%s' AND name='%s'",
		acct_table,
		rec->time_used,
		datebuf,
		rec->inbytes,
		rec->outbytes,
		rec->term_cause,
		rec->start,
		rec->id,
		rec->server,
		rec->name);
	} else {
		sprintf(buff_query,
		"UPDATE %s SET time_on=%lu,stop_time='%s',"
		"inbytes=%u,outbytes=%u,term_cause=%d,status=%d "
		"WHERE id='%s' AND server='%s' AND name='%s'",
	 	acct_table,
		rec->time_used,
		datebuf,
		rec->inbytes,
		rec->outbytes,
		rec->term_cause,
		rec->start,
		rec->id,
		rec->server,
		rec->name);
	}
	} 

		sql_query(buff_query,mysql);

	if (rec == NULL) return;
	if (mysql == NULL) return;

	if (rec->start != PW_STATUS_STOP &&
	    rec->start != PW_STATUS_ALIVE ){
#if 0
		DEBUG("no STOP or ALIVE packets - break");
#endif
		mysql_close(mysql);
		mysql = NULL;
	 return;
	}

	if (rec->name[0] == 0){
		 return;
	}

	if ((u_char)(rec->prefix-'A') <= (u_char)('Z'-'A')){
		while (group[igroup].gid != 0 &&
		    group[igroup].prefix != rec->prefix ) igroup++;
	} else {
		sprintf(buff_query,"SELECT gid FROM %s WHERE name='%s'",
			auth_table,
			rec->name);

	if(!sql_query(buff_query,mysql))
	  if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR,"MYSQL error: %s",mysql_error(mysql));
	    }else{
		if (mysql_num_rows(res) == 1){
			row = mysql_fetch_row(res);
			gid = atoi(row[0]);	
			mysql_free_result(res);
		}else{
		/*	row = mysql_fetch_row(res);*/
			mysql_free_result(res);
			mysql_close(mysql);
			mysql = NULL;
			return;
		}
	    }
		
		while (group[igroup].gid != 0 &&
		    gid != group[igroup].gid ) igroup++;
	} // no prefix but get GID from MySQL query
#if 1
	DEBUG("group.gid(%d) group.igroup(%d)", gid, igroup);
#endif

	if (rec->time_used <= group[igroup].freesec){
#if 1
	DEBUG("freesec used %d <= %d", rec->time_used, group[igroup].freesec);
#endif
		mysql_close(mysql);
		mysql = NULL;
		 return;
	}

	
	/*-------------------------------------
	 * type group - 0 unlimited
	 * type group - 4 daycost
	 */

	if (group[igroup].type == 0 || 
	    group[igroup].type == 4 ){
		mysql_close(mysql);
		mysql = NULL;
		return;
	}

#if 1 /* Limit tables */
	/*-------------------------------------
	 * check LIMIT per month
	 */
	 if (group[igroup].traf_s[0].bytes != 0){
		sprintf(buff_query,
		"SELECT SUM(time_on),SUM(inbytes),SUM(outbytes) "
		"FROM %s WHERE name='%s' AND"
		" date_format(start_time,'%%Y-%%m') = date_format(NOW(),'%%Y-%%m')",
#if 0
	" AND start_time >= CONCAT(YEAR(NOW()),'-',MONTH(NOW()),'-01 00:00:00')"
	" AND start_time <= ADDDATE(CONCAT(YEAR(NOW()),'-',MONTH(NOW()),'-01 00:00:00'), INTERVAL 1 MONTH)",
#endif
		acct_table,
		rec->name);

		if(sql_query(buff_query,mysql))
		 log(L_ERR, "MYSQL Error: Cannot Select");	

	 if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR, "MYSQL error: %s",mysql_error(mysql));
 	 } else {
		row = mysql_fetch_row(res);
		limit_timeon = atol(row[0]);
		limit_inbytes = atol(row[1]);
		limit_outbytes = atol(row[2]);

		DEBUG("MONTH: time_on=%d, inbytes=%d, outbytes=%d",
				limit_timeon, limit_inbytes, limit_outbytes);

		mysql_free_result(res);
	 }
	}
#endif
	/*-------------------------------------
	 * type group - 1 hourgroup 
	 * type group - 2 hourgroup + traffic
	 */

		sprintf(buff_query,
		"SELECT "
		"unix_timestamp(start_time),unix_timestamp(stop_time) "
		"FROM %s WHERE id='%s' AND server='%s' AND name='%s'",
		acct_table,
		rec->id,
		rec->server,
		rec->name);
		
		if(sql_query(buff_query,mysql))
		 log(L_ERR, "MYSQL Error: Cannot Select");	

	 if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR, "MYSQL error: %s",mysql_error(mysql));
 	 } else {
		u_int	time;
		row = mysql_fetch_row(res);
		start_time = atol(row[0]);
		stop_time  = atol(row[1]);

		mysql_free_result(res);

		if (hourgroup == 1 &&
 		   (group[igroup].type == 1 || 
		    group[igroup].type == 2) ){

		time = stop_time - ( start_time + rec->time_used );
#if 0
		DEBUG("time {(%ld - (%ld + %ld) + 30} %ld =? 60",
			stop_time, start_time, time, rec->time_used);
#endif

		if ((time + 30) > 60){
			log(L_ERR,"Incorrect Start/Stop Time %ld for [%s], id:%s",
				time,rec->name,rec->id);
			mysql_close(mysql);
			mysql = NULL;
			return;
		}

		ccostH = time2moneyH(start_time,stop_time,igroup);
#if 0
		DEBUG("sch %f ccH %f = time2moneyH(%ld,%ld)", 
			scosth, ccostH, start_time, stop_time);
#endif
		 } /* end type 1 & 2 */

	  }

		
	/*-----------------------------------
	 * type group - 2 hourgroup + traffic
	 * type group - 3 traffic
	 */

	if (group[igroup].type == 2 ||
	    group[igroup].type == 3 ){

	 double traf_cost;

	 inbytes = (rec->inbytes - sinbytes);
	 outbytes = (rec->outbytes - soutbytes);

	if (trafgroup == 1 && group[igroup].traf_cost == -1){ /* FIXX */
		traf_cost = time2moneyT(start_time, stop_time, igroup);
		DEBUG("traf_cost: T2M %d %.9f", __LINE__, traf_cost);
	} else {
		traf_cost = group[igroup].traf_cost/group[igroup].one_mb;
		DEBUG("traf_cost: G   %d %.9f", __LINE__, traf_cost);
	}
	DEBUG("traf_cost %d %.9f", __LINE__, traf_cost);

#if 1 /* debug */
	if (group[igroup].traf_s[0].bytes != 0){
		int i;
		long limit_bytes;

		//i = 0;

		switch (group[igroup].type_traf){
		case 0: /* in */
			limit_bytes = limit_inbytes;
			break;
		case 1: /* in+out */
			limit_bytes = (limit_inbytes + limit_outbytes);
			break;
		case 2: /* in ? out */
			limit_bytes = (limit_outbytes > limit_inbytes)?
					limit_outbytes : limit_inbytes;
			/* FIXX */
			break;
		case 3: /* out */
			limit_bytes = limit_outbytes;
			break;
		default:
			;
		}
			i = -1;

		do {
			i++;
			/* if begin not in table */
			if (group[igroup].traf_s[i].bytes > limit_bytes && i == 0){
#if 0 /* we already calculate */
				traf_cost = (group[igroup].traf_cost /
					     group[igroup].one_mb);
#endif
	DEBUG("traf_cost: [%d] findlimit !{ %d %.9f", i, __LINE__, traf_cost);
				i = -1; break;
			} else
#if 1
			if (group[igroup].traf_s[i].bytes > limit_bytes){
	DEBUG("traf_cost: [%d] findlimit  { %d %.9f", i, __LINE__, traf_cost);
					break;
			} else
#endif
			/* if end of table */
			if (group[igroup].traf_s[i].bytes == 0 && i > 0){
				traf_cost = (group[igroup].traf_s[i-1].cost /
					     group[igroup].one_mb);
	DEBUG("traf_cost: findlimit traf.bytes=%d traf.cost=%.9f onemb=%d",
				group[igroup].traf_s[i-1].bytes,
				group[igroup].traf_s[i-1].cost,
				group[igroup].one_mb);
	DEBUG("traf_cost: [%d] findlimit  } %d %.9f", i, __LINE__, traf_cost);
				i = -1; break; 
			} 
//		} while (i<10);
		} while (1);
//		} while (group[igroup].traf_s[i].bytes > limit_bytes);

	/*
	 *	(x3 - x1)
	 * y3 = --------- * (y2 - y1) + y1
	 *	(x2 - x1)
	 *
	 * cost -> y
	 * bytes -> x
	 *
	 * test A(10,30) B(75,15) C(45,[21.9])
 *
 * printf("%f\n", bytes2cost(1024*1024*5, 0.4,
 *			     1024*1024*10,0.3,
 *			     1024*1024*8, 1024*1024));
 * ~= 0.34
 *
 *	  (limit_bytes - bytes[i])
 * cost = ------------------------ * (cost[i+1]/mb - cost[i]/mb) + cost[i]/mb
 *	  (bytes[i+1] - bytes[i])
 *
	 */
#define bytes2cost(x1,y1,x2,y2,x3,mb)	\
((float)((x3)-(x1))/((x2)-(x1)) * (float)((y2)-(y1))/mb + (float)(y1)/mb)

		if (i == 0){
#if 0 /* we already calculate */
				traf_cost = (group[igroup].traf_cost /
					     group[igroup].one_mb);
#endif
	DEBUG("traf_cost: {we alredy calc!} %d %.9f", __LINE__, traf_cost);
		} else
		if (i != -1){

			if (downcost_type == 0){
		/* we are in table 0[i], 1[i+1]*/
		double x1,y1, x2,y2, x3,y3, mb;

		x1 = group[igroup].traf_s[i-1].bytes;
		y1 = group[igroup].traf_s[i-1].cost;
		x2 = group[igroup].traf_s[i].bytes;
		y2 = group[igroup].traf_s[i].cost;
		x3 = limit_bytes;
		mb = group[igroup].one_mb;
	DEBUG("traf_cost: aprox var x1=%.9f y1=%.9f x2=%.9f y2=%.9f x3=%.9f mb=%.9f",
		x1, y1, x2, y2, x3, mb);
#if 1
			traf_cost = ((float)(x3-x1)/(float)(x2-x1)) *
				     ((y2-y1)/(float)mb) + (y1/(float)mb);
	DEBUG("traf_cost: aprox %d %.9f", __LINE__, traf_cost);
#else
		/* define don't works correct :-(( why? i don't know...*/
			traf_cost = bytes2cost(group[igroup].traf_s[i].bytes,
					       group[igroup].traf_s[i].cost,
					       group[igroup].traf_s[i+1].bytes,
					       group[igroup].traf_s[i+1].cost,
					       limit_bytes,
					       group[igroup].one_mb);
#endif
			} else {
				traf_cost = (group[igroup].traf_s[i+1].cost /
					     group[igroup].one_mb);
	DEBUG("traf_cost: lin   %d %.9f", __LINE__, traf_cost);

			}
		}

	}
#endif /* debug */
#if 1
		DEBUG("%s %.9f %s in[%d]/out[%d] bytes", 
			trafgroup ? "trafgroup":"traf_cost", traf_cost,
			group[igroup].traf_s[0].bytes ? "limits":"",
			inbytes, outbytes);
#endif

	switch (group[igroup].type_traf){
	case 0: /* In */
	   	ccostT = (inbytes + 0.0) * traf_cost;
		break;
	case 1: /* In + Out */
	   	ccostT = ((outbytes + inbytes) + 0.0) * traf_cost; 
		break;
#if 1	/* FIXME !!! down know algoritm how to calculate Traffic */
	case 2: /* >? In | Out */
	  	 ccostT = (outbytes > inbytes) ? 
 			((outbytes + 0.0) * traf_cost): 
			((inbytes  + 0.0) * traf_cost);
		break;
#endif
	case 3: /* Out */
	   	ccostT = (outbytes + 0.0) * traf_cost;
		break;
	default: 	break;
	 } 
#if 1
		DEBUG("sct %.9f ccT %.9f type_traf %d traf_cost %.9f", 
			scostt, ccostT, group[igroup].type_traf, traf_cost);
#endif
	}

	/* ---------------------------------
	 * 	Calc cost(credit) and deposit !!
	 */

	sprintf(buff_query,
		"SELECT deposit,credit FROM %s WHERE name='%s'",
		auth_table,
		rec->name);

	if(!sql_query(buff_query,mysql))
	 if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)){
		log(L_ERR,"MYSQL error: %s",mysql_error(mysql));
 	 } else {
		row = mysql_fetch_row(res);
		deposit = atof(row[0]);
		credit = atof(row[1]);
		mysql_free_result(res);
	 }

	ccosth = (ccostH - scosth);
	ccostt = (ccostT + scostt); 
	dcost = ccosth + ccostT; /* delta */
	cost  = (deposit - dcost); /* new deposit!! */
#if 0
	if (cost <= 0.000000001 && cost > 0) /* don't want cost = 0.0000000001 */
		cost = 0;
#endif
#if 1
		DEBUG("deposit %.9f cost %.9f [dc %.9f cch %.9f cct %.9f]", 
			deposit, cost, dcost, ccosth, ccostt);
#endif
  	if ((ccosth + ccostt) != 0.0){

	sprintf(buff_query,
		"UPDATE %s SET cost='%4.9f',"
		"costh='%4.9f',costt='%4.9f',deposit='%4.9f' "
		"WHERE id='%s' AND server='%s' AND name='%s'",
		acct_table,
		(ccosth + ccostt),
		ccosth,
		ccostt,
		deposit,
		rec->id,
		rec->server,
		rec->name);

		sql_query(buff_query,mysql);

	sprintf(buff_query,
		"UPDATE %s SET deposit='%4.9f' WHERE name='%s'",
		auth_table,
		cost,
		rec->name);

		sql_query(buff_query,mysql);
  	}

	if (mysql != NULL)
		mysql_close(mysql);
	mysql = NULL;

	/* if user reach limit radius_xlat - kill user */
	if ((rec->start == PW_STATUS_ALIVE && 
	    group[igroup].type != 0 &&
	    (cost + credit) <= 0.01 ) ){
		log(L_INFO, "at NAS %s:%d kill user [%s]", 
					rec->server, rec->port, rec->name);
		sql_killuser(rec);
	}
	
	return; 
}

/*====================================================================
 *
 */

SQL_PWD	*sql_getpwname(char *name){

	static  SQL_PWD *sqlpwd;
	static  char 	lastname[32];
	static  time_t 	lasttime = 0;
	char	fname[32];
	char	query[256];
	float	deposit;
	time_t  now;
	int	login = 0;
	char	prefix = 0;
	int	i,in = 0;

	MYSQL_RES	*res;
	MYSQL_ROW	row;

	now = time(NULL);

 	if ((now <= lasttime + 5) && !strncmp(name,lastname,sizeof(lastname)))
		return sqlpwd;	

	strncpy(lastname,name,sizeof(lastname));

 	if (mysql == NULL){
		if (!(mysql = sql_reconnect())) return 0;	
	}

	strcpy(fname, name);

	if ((u_char)(name[0]-'A') <= (u_char)('Z'-'A')){
		prefix = name[0];
		strcpy(name, name + 1);
	}
	
	sprintf(query,"SELECT name,passwd,gid,deposit,unix_timestamp(expired),"
			"block,credit,exp_credit,allow_phone,"
		    	"unix_timestamp(date_on),callback,framed_ip,prefix "
			"FROM %s WHERE name = '%s'",
		auth_table, name);

	if(sql_query(query,mysql)) return NULL;

	if (!(res = mysql_store_result(mysql))){
		log(L_ERR,"MYSQL Error: Could get result for authentication");
		return NULL;
	}	

	if (mysql_num_rows(res) != 1){
		mysql_free_result(res);
		return NULL;
	}
	
	row = mysql_fetch_row(res);

	if (strcmp(name,row[0]) != 0){
		log(L_ERR,"%s must have trailing space in login attempt", name);
		return NULL;
	}	

	if ((sqlpwd = malloc(sizeof(SQL_PWD))) == NULL){
		log(L_ERR,"no memory");
		exit(1);
	}

	strcpy(sqlpwd->fname, fname);

	strcpy(sqlpwd->name, name);
	strcpy(sqlpwd->passwd, row[1]);
	sqlpwd->gid = atoi(row[2]);
	strcpy(sqlpwd->prefix, row[12]);
/*	
	DEBUG("copy ok sqlpw name %s gid %d prefix %s",
			sqlpwd->name,
			sqlpwd->gid,
			sqlpwd->prefix);
*/
//	if (strlen(sqlpwd->prefix) != 0 && prefix != 0){
	if (prefix != 0){
	    for ( i = 0; i < strlen(sqlpwd->prefix); i++){
/*		DEBUG("for len(prefix)..[%c] prefix [%c] gid %d",
				sqlpwd->prefix[i],prefix,group[in].gid);
*/
		if (sqlpwd->prefix[i] == prefix)
		    do {
//		DEBUG("cmp prefix [%c] grp.prefix [%c]",prefix,group[in].prefix);
		 	if (prefix == group[in].prefix){
			    sqlpwd->gid = group[in].gid;
			    sqlpwd->igroup = in;
			    sqlpwd->type = group[in].type;
			    login = 1;
			 break;
		        }
		      } while(group[++in].gid != 0);
	    }
	} else {
		do {
			if (sqlpwd->gid == group[in].gid){
			    sqlpwd->igroup = in;
			    sqlpwd->type = group[in].type;
		            login = 1;
			 break;
		        }
		} while(group[++in].gid != 0);
	}

	if (login == 1){

	sqlpwd->block = atol(row[5]);
	strcpy(sqlpwd->allow_phone, row[8]);
	sqlpwd->activ = atol(row[9]);
	deposit = atof(row[3]);
	sqlpwd->money = deposit + atof(row[6]);
	sqlpwd->time = atol(row[4]) + atoi(row[7]);
	strcpy(sqlpwd->callback, row[10]);	
 	strcpy(sqlpwd->ip, row[11]);

 	mysql_free_result(res);	
	mysql_close(mysql);
	mysql = NULL;
	lasttime = now;
/*
	DEBUG("sql_pw->name %s",sqlpwd->name);
	DEBUG("sql_pw->gid %d",sqlpwd->gid);
	DEBUG("sql_pw->money %.4f",sqlpwd->money);
	DEBUG("sql_pw->time %d",sqlpwd->time);
	DEBUG("sql_pw->activ %d",sqlpwd->activ);
	DEBUG("sql_pw->block %d",sqlpwd->block);
	DEBUG("sql_pw->igroup %d",sqlpwd->igroup);
	DEBUG("sql_pw->type %d",sqlpwd->type);
*/
 		return sqlpwd;	

	} else {

		mysql_free_result(res);
		return NULL;

	}
}

/*======================================================================
 *
 */
#ifdef CREATE_NAME_TABLE
int	sql_create_name_table(char *name){

	MYSQL_RES	*res;
	MYSQL_ROW	row;
	SESSION_REC	fix_rec;

	char		buff_query[256*2];


	if (mysql == NULL)
		mysql = sql_reconnect();


		sprintf(buff_query,
		"CREATE TABLE IF NOT EXISTS %s ("
			" id varchar(32) NOT NULL default '',"
			" time_on int(11) NOT NULL default '0',"
			" inbytes int(11) default NULL,"
			" outbytes int(11) default NULL,"
#ifdef  STOP_TIME
			" stop_time datetime NOT NULL default '0000-00-00 00:00:00',"
#endif
			" start_time datetime NOT NULL default '2001-01-01 00:00:00'"
			" ) TYPE=MyISAM;",
			name
			);

		DEBUG("%s", buff_query);

	     if (sql_query(buff_query,mysql))
		 log(L_ERR,"MYSQL Error: Cannot CREATE");	

	if (mysql != NULL)
		mysql_close(mysql);
	mysql = NULL;
}
#endif
/*======================================================================
 *
 *	not use - too old style
 */
#if 0
int	sql_crypt(char *name,char *string){

	SQL_PWD		*sql_pwd;
	time_t		now;

//	DEBUG("sql_crypt %s",name);
		now = time(0);
	if ((sql_pwd = sql_getpwname(name)) == NULL)
		return	-1;
	if (strcmp(sql_pwd->passwd,crypt(string,sql_pwd->passwd)))
		return	-1;
//	if ((sql_pwd->type == 1 || sql_pwd->type == 2) && 
//		money2time(sql_pwd->money,sql_pwd->igroup) <= 0)
//		return	-10;
//	if ((int)(sql_pwd->money * 100) <= 0 && sql_pwd->type != 0)
	if (sql_pwd->money <= 0.01 && sql_pwd->type != 0)
		return	-10;
	if (sql_pwd->activ > now)
		return -9;
	if (sql_pwd->block == 1)
		return -6;
//	if (sql_pwd->time != 0 && sql_pwd->time > now && 
//		(sql_pwd->type == 0 || sql_pwd->type == 3))
//		return -7;
	
	DEBUG("User %s successfully authenticated via mysql", name);
		return	0;	
}
#endif
/*====================================================================
 *	run build query and get 1 result from his
 */

int	sql_run_query(const char *query){

	MYSQL_RES *res;
	MYSQL_ROW row;
	int	r = 0;
	
	if (mysql == NULL)
		mysql = sql_reconnect();

	   sql_query(query, mysql);
	if ((res = mysql_store_result(mysql)) == NULL)
		if (mysql_field_count(mysql) != NULL){
		   log(L_ERR,"MYSQL error: %s", mysql_error(mysql));
		   return r;
		}
		row = mysql_fetch_row(res);
	if (row[0] == 0)
		r = 0;
	else    r = atol(row[0]);

	mysql_free_result(res);
	mysql_close(mysql);
	mysql = NULL;
	return r;
}


/*===================================================================
 * 	diff_time Hourgroup
 *
 */

static double time2moneyH(long start_time, long stop_time, u_int igroup){

	u_int		stop_yday,stop_wday,stop_sec,stop_min,stop_hour;
	u_int		start_yday,start_wday,start_sec,start_min,start_hour;
	u_int		t_all = 0;
	struct tm 	*tm_tmp;
	u_int		ihl = 0;
	double		cost = 0;
	u_int		hour,wday,yday,cwday;

		tm_tmp = localtime(&start_time);
		start_hour = tm_tmp->tm_hour;
		start_min  = tm_tmp->tm_min;
		start_sec  = tm_tmp->tm_sec;
		start_wday = tm_tmp->tm_wday;
		start_yday = tm_tmp->tm_yday + 1;
#if 0
	DEBUG("%2d:%2d:%2d w[%2d] y[%2d]",
		start_hour, start_min, start_sec, start_wday, start_yday);
#endif
		tm_tmp = localtime(&stop_time);
		stop_hour = tm_tmp->tm_hour;
		stop_min  = tm_tmp->tm_min;
		stop_sec  = tm_tmp->tm_sec;
		stop_wday = tm_tmp->tm_wday;
		stop_yday = tm_tmp->tm_yday + 1;
#if 0
	DEBUG("%2d:%2d:%2d w[%2d] y[%2d]",
		stop_hour, stop_min, stop_sec, stop_wday, stop_yday);
#endif

	hour = start_hour;
	wday = start_wday;
	yday = start_yday;

	do {
		if (hour == 24){
			hour = 0;
			wday ++; 
			yday ++;
			if (wday == 7) wday = 0;
			ihl = 0;
#if 0
		DEBUG("new day is come %d", wday);
#endif
		}

		if (ihl == 0){
			ihl = 0;
			while (holiday_table[ihl] != 0){
				if (holiday_table[ihl] == yday){ 
					ihl = 8; break; }
						    else ihl++; 
			}
			ihl = 4;
		}

		cwday = (ihl == 8) ? ihl : wday;

			/*======[ in One Hour ]======*/
	   if (start_hour == stop_hour && start_yday == stop_yday){
		t_all = (stop_min - start_min) * 60 - start_sec + stop_sec;
		cost += t_all * group[igroup].hour[cwday][hour]/3600;
#if 0
		DEBUG("start_hour %d == stop_hour %d (%d) [%f] %f", 
		start_hour, hour, t_all, group[igroup].hour[cwday][hour], cost);
#endif
		return cost;
	   } else
			/*======[ from Start to ...]=======*/
	   if (start_hour == hour && start_yday == yday){
		t_all = (59 - start_min) * 60 + (60 - start_sec); 
		cost += t_all * group[igroup].hour[cwday][hour]/3600;
#if 0
		DEBUG("start_hour %d == hour %d (%d) [%f] %f", 
		start_hour, hour, t_all, group[igroup].hour[cwday][hour], cost);
#endif
	   } else
			/*======[ ... to Stop  ]======*/
	   if (stop_hour == hour && stop_yday == yday){
		t_all = stop_min * 60 + stop_sec;
		cost += t_all * group[igroup].hour[cwday][hour]/3600;
#if 0
		DEBUG("stop_hour %d == hour %d (%d) [%f] %f", 
		stop_hour, hour, t_all, group[igroup].hour[cwday][hour], cost);
#endif
	   } else {
		cost += group[igroup].hour[cwday][hour];
#if 0
		DEBUG("else hour %d (%d) group[%d].hour[%d+%d] [%f] %f", hour,
		t_all, igroup, cwday,hour,group[igroup].hour[cwday][hour],cost);
#endif
		}
	hour++;
	} while((yday != stop_yday) || (hour != (stop_hour+1)) );
#if 0	
		DEBUG("return cost %f", cost);
#endif
	return cost;
}

/*===================================================================
 * 	diff_time Trafgroup
 *
 */

static double time2moneyT(long start_time, long stop_time, u_int igroup){

	u_int		stop_yday,stop_wday,stop_sec,stop_min,stop_hour;
	u_int		start_yday,start_wday,start_sec,start_min,start_hour;
	u_int		t_all = 0;
	struct tm 	*tm_tmp;
	u_int		ihl = 0;
	double		cost = 0;
	u_int		hour,wday,yday,cwday;

		tm_tmp = localtime(&start_time);
		start_hour = tm_tmp->tm_hour;
		start_min  = tm_tmp->tm_min;
		start_sec  = tm_tmp->tm_sec;
		start_wday = tm_tmp->tm_wday;
		start_yday = tm_tmp->tm_yday + 1;
#if 0
	DEBUG("%2d:%2d:%2d w[%2d] y[%2d]",
		start_hour, start_min, start_sec, start_wday, start_yday);
#endif
		tm_tmp = localtime(&stop_time);
		stop_hour = tm_tmp->tm_hour;
		stop_min  = tm_tmp->tm_min;
		stop_sec  = tm_tmp->tm_sec;
		stop_wday = tm_tmp->tm_wday;
		stop_yday = tm_tmp->tm_yday + 1;
#if 0
	DEBUG("%2d:%2d:%2d w[%2d] y[%2d]",
		stop_hour, stop_min, stop_sec, stop_wday, stop_yday);
#endif


	hour = stop_hour;
	wday = stop_wday;
	yday = stop_yday;
#if 0
	do {
#endif
		if (hour == 24){
			hour = 0;
			wday ++; 
			yday ++;
			if (wday == 7) wday = 0;
			ihl = 0;
#if 0
		DEBUG("new day is come %d", wday);
#endif
		}

		if (ihl == 0){
			ihl = 0;
			while (holiday_table[ihl] != 0){
				if (holiday_table[ihl] == yday){ 
					ihl = 8; break; }
				else ihl++; 
			}
			ihl = 4;
		}

		cwday = (ihl == 8) ? ihl : wday;

		cost = (double)group[igroup].traf[cwday][hour];
		cost = (double)cost/(double)group[igroup].one_mb; /* 1048576; */
		return cost;

	   if (start_hour == stop_hour && start_yday == stop_yday){
		cost = group[igroup].traf[cwday][hour]/group[igroup].one_mb; /* 1048576; */
#if 0
		t_all = (stop_min - start_min) * 60 - start_sec + stop_sec;
		cost += t_all * group[igroup].hour[cwday][hour]/3600;
#endif
#if 0
		DEBUG("start_hour %d == stop_hour %d (%d) [%f] %f", start_hour,hour,t_all,group[igroup].hour[cwday][hour],cost);
#endif
		return cost;
	   } else

	   if (start_hour == hour && start_yday == yday){
#if 0
		t_all = (59 - start_min) * 60 + (60 - start_sec); 
		cost += t_all * group[igroup].hour[cwday][hour]/3600;
#endif
#if 0
		DEBUG("start_hour %d == hour %d (%d) [%f] %f", 
		start_hour, hour, t_all, group[igroup].hour[cwday][hour], cost);
#endif
	   } else

	   if (stop_hour == hour && stop_yday == yday){
		t_all = stop_min * 60 + stop_sec;
		cost += t_all * group[igroup].hour[cwday][hour]/3600;
#if 0
		DEBUG("stop_hour %d == hour %d (%d) [%f] %f", 
		stop_hour, hour,t_all,group[igroup].hour[cwday][hour],cost);
#endif
	   } else {
		cost += group[igroup].hour[cwday][hour];
#if 0
		DEBUG("else hour %d (%d) group[%d].hour[%d+%d] [%f] %f", hour,
		t_all, igroup, cwday,hour,group[igroup].hour[cwday][hour],cost);
#endif
		}
#if 0
	hour++;
	}while((yday != stop_yday) || (hour != (stop_hour+1)) );
#endif

#if 0	
		DEBUG("ret cost %f",cost);
#endif
	return cost;
}

/*=================================================================
 *
 *
 */

long	money2time(double money, u_int igroup){

	u_int		yday,wday,sec,min,hour;
	struct tm 	*tm_tmp;
	time_t		curtime;
	int		ihl = 0;
	u_int		cwday;
	double		cost,cmoney = money;
	long		timeout = 0;

		curtime = time(NULL);

		tm_tmp = localtime(&curtime);
		hour = tm_tmp->tm_hour;
		min  = tm_tmp->tm_min;
		sec  = tm_tmp->tm_sec;
		wday = tm_tmp->tm_wday;
		yday = tm_tmp->tm_yday + 1;

	while(1) {

		if (hour > 23){
			 hour = 0;
			 wday ++;
			 yday ++;
		if (wday == 7) wday = 0;
			ihl = 0;
#if 0
		DEBUG("new day is come %d", wday);
#endif
		}

		if (ihl == 0){
			ihl = 0;
			while (holiday_table[ihl] != 0){
				if (holiday_table[ihl] == yday){ 
					ihl = 8; break; }
			    	else ihl++; 
			}
			ihl = 4;
		}

		cwday = (ihl == 8) ? ihl : wday;

		cost = group[igroup].hour[cwday][hour];
#if 0
		DEBUG("/ money %f cost %f timeout %d", cmoney, cost, timeout);
#endif		
		if ((cmoney - cost) > 0.0){
			cmoney -= cost;
			timeout += 3600;
			hour ++;
			continue;
		} else {
#if 0
		DEBUG("/60 money %f cost %f timeout %d", cmoney, cost, timeout);
#endif
			while((cmoney - cost/60.0) > 0.0){
				cmoney -= (cost/60.0);
				timeout += 60;
			}
#if 0
		DEBUG("/3600 money %f cost %f timeout %d", cmoney, cost, timeout);
#endif
			while((cmoney - cost/3600.0) > 0.0){
				cmoney -= (cost/3600.0);
				timeout ++;
			}
#if 0
		DEBUG("end money %f cost %f timeout %d", cmoney, cost, timeout);
#endif
		}
			return timeout;
	}
}

#endif
