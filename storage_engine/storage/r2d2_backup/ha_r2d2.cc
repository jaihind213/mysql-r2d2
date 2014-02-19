/* Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#include "sql_priv.h"
#include "unireg.h"
#include "probes_mysql.h"
#include "ha_r2d2.h"
#include "sql_class.h"                          // THD, SYSTEM_THREAD_SLAVE_SQL

/* Static declarations for handlerton */

static handler *r2d2_create_handler(handlerton *hton,
                                         TABLE_SHARE *table,
                                         MEM_ROOT *mem_root)
{
  return new (mem_root) ha_r2d2(hton, table);
}


/* Static declarations for shared structures */

static mysql_mutex_t r2d2_mutex;
static HASH r2d2_open_tables;

static st_r2d2_share *get_share(const char *table_name);
static void free_share(st_r2d2_share *share);

/*****************************************************************************
** r2d2 tables
*****************************************************************************/

ha_r2d2::ha_r2d2(handlerton *hton,
                           TABLE_SHARE *table_arg)
  :handler(hton, table_arg)
{}


static const char *ha_r2d2_exts[] = {
  NullS
};

const char **ha_r2d2::bas_ext() const
{
  return ha_r2d2_exts;
}

int ha_r2d2::open(const char *name, int mode, uint test_if_locked)
{
  DBUG_ENTER("ha_r2d2::open");

  if (!(share= get_share(name)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  thr_lock_data_init(&share->lock, &lock, NULL);
  DBUG_RETURN(0);
}

int ha_r2d2::close(void)
{
  DBUG_ENTER("ha_r2d2::close");
  free_share(share);
  DBUG_RETURN(0);
}

int ha_r2d2::create(const char *name, TABLE *table_arg,
                         HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_r2d2::create");
  DBUG_RETURN(0);
}

/*
  Intended to support partitioning.
  Allows a particular partition to be truncated.
*/
int ha_r2d2::truncate()
{
  DBUG_ENTER("ha_r2d2::truncate");
  DBUG_RETURN(0);
}

const char *ha_r2d2::index_type(uint key_number)
{
  DBUG_ENTER("ha_r2d2::index_type");
  DBUG_RETURN((table_share->key_info[key_number].flags & HA_FULLTEXT) ? 
              "FULLTEXT" :
              (table_share->key_info[key_number].flags & HA_SPATIAL) ?
              "SPATIAL" :
              (table_share->key_info[key_number].algorithm ==
               HA_KEY_ALG_RTREE) ? "RTREE" : "BTREE");
}

int ha_r2d2::write_row(uchar * buf)
{
  DBUG_ENTER("ha_r2d2::write_row");
  DBUG_RETURN(table->next_number_field ? update_auto_increment() : 0);
}

int ha_r2d2::update_row(const uchar *old_data, uchar *new_data)
{
  DBUG_ENTER("ha_r2d2::update_row");
  THD *thd= ha_thd();
  if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL && thd->query() == NULL)
    DBUG_RETURN(0);
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_r2d2::delete_row(const uchar *buf)
{
  DBUG_ENTER("ha_r2d2::delete_row");
  THD *thd= ha_thd();
  if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL && thd->query() == NULL)
    DBUG_RETURN(0);
  DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_r2d2::rnd_init(bool scan)
{
  DBUG_ENTER("ha_r2d2::rnd_init");
  DBUG_RETURN(0);
}


int ha_r2d2::rnd_next(uchar *buf)
{
  int rc;
  DBUG_ENTER("ha_r2d2::rnd_next");
  MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                       TRUE);
  THD *thd= ha_thd();
  if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL && thd->query() == NULL)
    rc= 0;
  else
    rc= HA_ERR_END_OF_FILE;
  MYSQL_READ_ROW_DONE(rc);
  table->status= rc ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(rc);
}


int ha_r2d2::rnd_pos(uchar * buf, uchar *pos)
{
  DBUG_ENTER("ha_r2d2::rnd_pos");
  MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
                       FALSE);
  DBUG_ASSERT(0);
  MYSQL_READ_ROW_DONE(0);
  DBUG_RETURN(0);
}


void ha_r2d2::position(const uchar *record)
{
  DBUG_ENTER("ha_r2d2::position");
  DBUG_ASSERT(0);
  DBUG_VOID_RETURN;
}


int ha_r2d2::info(uint flag)
{
  DBUG_ENTER("ha_r2d2::info");

  bzero((char*) &stats, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  DBUG_RETURN(0);
}

int ha_r2d2::external_lock(THD *thd, int lock_type)
{
  DBUG_ENTER("ha_r2d2::external_lock");
  DBUG_RETURN(0);
}


THR_LOCK_DATA **ha_r2d2::store_lock(THD *thd,
                                         THR_LOCK_DATA **to,
                                         enum thr_lock_type lock_type)
{
  DBUG_ENTER("ha_r2d2::store_lock");
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK TABLE or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !thd_in_lock_tables(thd)
        && !thd_tablespace_op(thd))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT && !thd_in_lock_tables(thd))
      lock_type = TL_READ;

    lock.type= lock_type;
  }
  *to++= &lock;
  DBUG_RETURN(to);
}


int ha_r2d2::index_read_map(uchar * buf, const uchar * key,
                                 key_part_map keypart_map,
                             enum ha_rkey_function find_flag)
{
  int rc;
  DBUG_ENTER("ha_r2d2::index_read");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  THD *thd= ha_thd();
  if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL && thd->query() == NULL)
    rc= 0;
  else
    rc= HA_ERR_END_OF_FILE;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  table->status= rc ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(rc);
}


int ha_r2d2::index_read_idx_map(uchar * buf, uint idx, const uchar * key,
                                 key_part_map keypart_map,
                                 enum ha_rkey_function find_flag)
{
  int rc;
  DBUG_ENTER("ha_r2d2::index_read_idx");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  THD *thd= ha_thd();
  if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL && thd->query() == NULL)
    rc= 0;
  else
    rc= HA_ERR_END_OF_FILE;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  table->status= rc ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(rc);
}


int ha_r2d2::index_read_last_map(uchar * buf, const uchar * key,
                                      key_part_map keypart_map)
{
  int rc;
  DBUG_ENTER("ha_r2d2::index_read_last");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  THD *thd= ha_thd();
  if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL && thd->query() == NULL)
    rc= 0;
  else
    rc= HA_ERR_END_OF_FILE;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  table->status= rc ? STATUS_NOT_FOUND : 0;
  DBUG_RETURN(rc);
}


int ha_r2d2::index_next(uchar * buf)
{
  int rc;
  DBUG_ENTER("ha_r2d2::index_next");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc= HA_ERR_END_OF_FILE;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(rc);
}


int ha_r2d2::index_prev(uchar * buf)
{
  int rc;
  DBUG_ENTER("ha_r2d2::index_prev");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc= HA_ERR_END_OF_FILE;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(rc);
}


int ha_r2d2::index_first(uchar * buf)
{
  int rc;
  DBUG_ENTER("ha_r2d2::index_first");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc= HA_ERR_END_OF_FILE;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(rc);
}


int ha_r2d2::index_last(uchar * buf)
{
  int rc;
  DBUG_ENTER("ha_r2d2::index_last");
  MYSQL_INDEX_READ_ROW_START(table_share->db.str, table_share->table_name.str);
  rc= HA_ERR_END_OF_FILE;
  MYSQL_INDEX_READ_ROW_DONE(rc);
  table->status= STATUS_NOT_FOUND;
  DBUG_RETURN(rc);
}


static st_r2d2_share *get_share(const char *table_name)
{
  st_r2d2_share *share;
  uint length;

  length= (uint) strlen(table_name);
  mysql_mutex_lock(&r2d2_mutex);
    
  if (!(share= (st_r2d2_share*)
        my_hash_search(&r2d2_open_tables,
                       (uchar*) table_name, length)))
  {
    if (!(share= (st_r2d2_share*) my_malloc(sizeof(st_r2d2_share) +
                                                 length,
                                                 MYF(MY_WME | MY_ZEROFILL))))
      goto error;

    share->table_name_length= length;
    strmov(share->table_name, table_name);
    
    if (my_hash_insert(&r2d2_open_tables, (uchar*) share))
    {
      my_free(share);
      share= NULL;
      goto error;
    }
    
    thr_lock_init(&share->lock);
  }
  share->use_count++;
  
error:
  mysql_mutex_unlock(&r2d2_mutex);
  return share;
}

static void free_share(st_r2d2_share *share)
{
  mysql_mutex_lock(&r2d2_mutex);
  if (!--share->use_count)
    my_hash_delete(&r2d2_open_tables, (uchar*) share);
  mysql_mutex_unlock(&r2d2_mutex);
}

static void r2d2_free_key(st_r2d2_share *share)
{
  thr_lock_delete(&share->lock);
  my_free(share);
}

static uchar* r2d2_get_key(st_r2d2_share *share, size_t *length,
                                my_bool not_used __attribute__((unused)))
{
  *length= share->table_name_length;
  return (uchar*) share->table_name;
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key bh_key_mutex_r2d2;

static PSI_mutex_info all_r2d2_mutexes[]=
{
  { &bh_key_mutex_r2d2, "r2d2", PSI_FLAG_GLOBAL}
};

void init_r2d2_psi_keys()
{
  const char* category= "r2d2";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_r2d2_mutexes);
  PSI_server->register_mutex(category, all_r2d2_mutexes, count);
}
#endif

static int r2d2_init(void *p)
{
  handlerton *r2d2_hton;

#ifdef HAVE_PSI_INTERFACE
  init_r2d2_psi_keys();
#endif

  r2d2_hton= (handlerton *)p;
  r2d2_hton->state= SHOW_OPTION_YES;
  r2d2_hton->db_type= DB_TYPE_BLACKHOLE_DB;
  r2d2_hton->create= r2d2_create_handler;
  r2d2_hton->flags= HTON_CAN_RECREATE;

  mysql_mutex_init(bh_key_mutex_r2d2,
                   &r2d2_mutex, MY_MUTEX_INIT_FAST);
  (void) my_hash_init(&r2d2_open_tables, system_charset_info,32,0,0,
                      (my_hash_get_key) r2d2_get_key,
                      (my_hash_free_key) r2d2_free_key, 0);

  return 0;
}

static int r2d2_fini(void *p)
{
  my_hash_free(&r2d2_open_tables);
  mysql_mutex_destroy(&r2d2_mutex);

  return 0;
}

struct st_mysql_storage_engine r2d2_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(r2d2)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &r2d2_storage_engine,
  "r2d2",
  "vishnu rao",
  "Simple storage engine for MySQL to interace with message queues. (R2D2 take this message to Obi-Wan Kenobi. may d force b with u)",
  PLUGIN_LICENSE_GPL,
  r2d2_init, /* Plugin Init */
  r2d2_fini, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;
