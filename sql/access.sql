GRANT USAGE ON data.* to billing@localhost;
GRANT CREATE,SELECT,UPDATE,INSERT,DELETE ON data.* to billing@localhost;
SET PASSWORD FOR "billing"@"localhost"=PASSWORD("test123");
