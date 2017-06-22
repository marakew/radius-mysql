CREATE TABLE IF NOT EXISTS history (
  name varchar(32) NOT NULL default '',
  date datetime NOT NULL default '0000-00-00 00:00:00',
  type smallint(1) NOT NULL default '0',
  action varchar(100) NOT NULL default '',
  operator varchar(32) NOT NULL default ''
) TYPE=MyISAM;
