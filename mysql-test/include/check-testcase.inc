# ==== Purpose ====
#
# This test is executed twice for each test case. Before every
# testcase it is run with mysqltest in record mode and will thus
# produce an output file that can be compared to output from after the
# testcase. In that way, it is possible to check that a testcase does
# not have any unwanted side effects..

--disable_query_log

# We want to ensure all slave configuration is restored.  But SHOW
# SLAVE STATUS returns nothing for servers not configured as slaves,
# and (after BUG#28796 was fixed) there is no way to de-configure a
# slave.  Hence, it's impossible to clean up the replication state at
# the end.  But we want to check that the slave is stopped, there is
# no error, and a few other invariants.  So we issue SHOW SLAVE
# STATUS, and if it returns no rows we fake the result.
--let $tmp= query_get_value(SHOW SLAVE STATUS, Slave_IO_Running, 1)
--let $tmp= `SELECT '$tmp' = 'No such row'`
if ($tmp)
{
  # Note: after WL#5177, fields 13-18 shall not be filtered-out.
  --echo Slave_IO_State	
  --echo Master_Host	127.0.0.1
  --echo Master_User	root
  --echo Master_Port	#
  --echo Connect_Retry	#
  --echo Master_Log_File	#
  --echo Read_Master_Log_Pos	#
  --echo Relay_Log_File	#
  --echo Relay_Log_Pos	#
  --echo Relay_Master_Log_File	#
  --echo Slave_IO_Running	No
  --echo Slave_SQL_Running	No
  --echo Replicate_Do_DB	#
  --echo Replicate_Ignore_DB	#
  --echo Replicate_Do_Table	#
  --echo Replicate_Ignore_Table	#
  --echo Replicate_Wild_Do_Table	#
  --echo Replicate_Wild_Ignore_Table	#
  --echo Last_Errno	0
  --echo Last_Error	
  --echo Skip_Counter	0
  --echo Exec_Master_Log_Pos	#
  --echo Relay_Log_Space	#
  --echo Until_Condition	#
  --echo Until_Log_File	#
  --echo Until_Log_Pos	#
  --echo Master_SSL_Allowed	Yes
  --echo Master_SSL_CA_File	
  --echo Master_SSL_CA_Path	
  --echo Master_SSL_Cert	
  --echo Master_SSL_Cipher	
  --echo Master_SSL_Key	
  --echo Seconds_Behind_Master	NULL
  --echo Master_SSL_Verify_Server_Cert	No
  --echo Last_IO_Errno	0
  --echo Last_IO_Error	
  --echo Last_SQL_Errno	0
  --echo Last_SQL_Error	
  --echo Replicate_Ignore_Server_Ids	
  --echo Master_Server_Id	#
  --echo Master_SSL_Crl	#
  --echo Master_SSL_Crlpath	#
  --echo Using_Gtid	Slave_Pos
  --echo Gtid_IO_Pos	#
  --echo Replicate_Do_Domain_Ids	
  --echo Replicate_Ignore_Domain_Ids	
  --echo Parallel_Mode	optimistic
  --echo SQL_Delay	0
  --echo SQL_Remaining_Delay	NULL
  --echo Slave_SQL_Running_State	
  --echo Slave_DDL_Groups	#
  --echo Slave_Non_Transactional_Groups	#
  --echo Slave_Transactional_Groups	#
  --echo Replicate_Rewrite_DB	#
  --echo Connects_Tried	#
  --echo Master_Retry_Count	#
}
if (!$tmp) {
  # Note: after WL#5177, fields 13-18 shall not be filtered-out.
  --replace_column 4 # 5 # 6 # 7 # 8 # 9 # 10 # 13 # 14 # 15 # 16 # 17 # 18 # 22 # 23 # 24 # 25 # 26 # 40 # 41 # 42 # 44 # 51 # 52 # 53 # 54 # 55 # 56 #
  query_vertical
  SHOW SLAVE STATUS;
}
#
# Note, we must never, _ever_, add extra rows to this output of SHOW SLAVE
# STATUS, except at the very end, as this breaks backwards compatibility
# with applications or scripts that parse the output. This also means that
# we cannot add _any_ new rows in a GA version if a different row was
# already added in a later MariaDB version, as this would make it impossible
# to merge the change up while preserving the order of rows.
#

#
# Ensure that we don't get warnings from mysql.proc (used by check_mysqld)
#

--disable_query_log
--disable_warnings
--disable_result_log
select count(*) from mysql.proc;
--enable_result_log
--enable_warnings
--enable_query_log

call mtr.check_testcase();

let $datadir=`select @@datadir`;
list_files $datadir mariadb_upgrade_info;
list_files $datadir/test #sql*;
list_files $datadir/mysql #sql*;

#
# Check that SHOW ENGINE INNODB STATUS does not show any active transactions
# We do this only if wsrep provider is loaded, to avoid disturbing any non-Galera MTR tests
#
if (`SELECT COUNT(*)=1 FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME = 'wsrep' AND PLUGIN_STATUS='ACTIVE'`) {
  if (`SELECT @@wsrep_on`) {
    if (`SELECT COUNT(*) FROM information_schema.innodb_trx WHERE trx_mysql_thread_id != 0`) {
      if ($before) {
        --echo Before test start.
      }
      if (!$before) {
        --echo After test end.
      }
      --echo There is one or more active InnoDB transaction(s) when there should be none. Dumping some diagnostics.

      --let $status_locks = `SELECT @@innodb_status_output_locks`
      --let $status_output = `SELECT @@innodb_status_output`
      --enable_query_log
      SET GLOBAL innodb_status_output_locks=ON;
      SHOW ENGINE INNODB STATUS;
      --disable_query_log
      --eval SET GLOBAL innodb_status_output_locks=$status_locks;
      --eval SET GLOBAL innodb_status_output=$status_output;
      --enable_query_log

      --vertical_results
      if ($before) {
        --replace_regex /$/ /
      }
      SELECT * FROM information_schema.processlist;

      if ($before) {
        --replace_regex /$/ /
      }
      SELECT * FROM information_schema.innodb_trx;

      if ($before) {
        --replace_regex /$/ /
      }
      SELECT * FROM information_schema.innodb_locks;

      if ($before) {
        --replace_regex /$/ /
      }
      SELECT * FROM information_schema.innodb_lock_waits;
      --horizontal_results
      --disable_query_log
    }
  }
}

--enable_query_log
