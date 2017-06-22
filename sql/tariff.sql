CREATE TABLE IF NOT EXISTS tariff (
	gid int(6) NOT NULL default '9999',
	type smallint(1) NOT NULL default '0',
	bytes int(11) NOT NULL default '0',
	cost float NOT NULL default '0'
) TYPE=MyISAM;
