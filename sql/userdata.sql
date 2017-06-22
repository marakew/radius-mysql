CREATE TABLE IF NOT EXISTS userdata (

	name varchar(32) NOT NULL default '',
	passwd varchar(32) NOT NULL default '*',
	crypt tinyint(1) NOT NULL default '0',
	gid smallint(5) NOT NULL default '9999',
	deposit double NOT NULL default '0.000000',
	credit smallint(5) NOT NULL default '0',
	rname varchar(100) NOT NULL default '',

	mail_cost double NOT NULL default '0',
	mail varchar(30) NOT NULL default '',

	expired datetime NOT NULL default '0000-00-00 00:00:00',
	exp_credit smallint(5) NOT NULL default '0',
	block smallint(1) NOT NULL default '0',
	callback varchar(15) NOT NULL default '',
	prefix varchar(10) NOT NULL default '',
	framed_ip varchar(16) NOT NULL default '',
	allow_phone varchar(150) NOT NULL default '',
	address varchar(150) NOT NULL default '',
	phone varchar(30) NOT NULL default '',
	date_on date NOT NULL default '0000-00-00',
	prim text default NULL,
	PRIMARY KEY (name)
) TYPE=MyISAM;
