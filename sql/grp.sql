CREATE TABLE IF NOT EXISTS grp (
	prefix char(1) NOT NULL default '',
	gid int(6) NOT NULL default '9999',
	grpname varchar(32) NOT NULL default 'none',
	rname varchar(64) NOT NULL default '*',
	freesec smallint(5) NOT NULL default '0',
	type smallint(1) NOT NULL default '0',
	traf_cost float NOT NULL default '0',
	1mb int(8) NOT NULL default '1048576',
	daycost float NOT NULL default '0',
	use_callback smallint(1) NOT NULL default '0',
	type_traf smallint(1) NOT NULL default '0',
	PRIMARY KEY (gid)
) TYPE=MyISAM;
