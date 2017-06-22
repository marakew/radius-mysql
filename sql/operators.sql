CREATE TABLE IF NOT EXISTS operators (
  name varchar(32) NOT NULL default '',
  passwd varchar(13) NOT NULL default '*',
  access smallint(1) NOT NULL default '0',
  rname varchar(100) NOT NULL default '',
  PRIMARY KEY (name)
) TYPE=MyISAM;
