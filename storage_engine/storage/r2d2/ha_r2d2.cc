/* Copyright (c) todo */


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#define MYSQL_SERVER 1
#define ENGINE_INIT_FAILED -1
#define ENGINE_INIT_SUCCESS 0
#define MAX_MSG_SIZE 10240

#define MSG_PAYLOAD_COLUMN "payload"

#define FACTORY_CLASS "r2d2/msg/MessengerFactory"
#define MESSENGER_GETTER_METHOD "getMessenger"
#define MESSENGER_GETTER_METHOD_SIGNATURE "(Ljava/lang/String;)Lr2d2/msg/Messenger;"
#define MESSENGER_DESTROY_METHOD "destroyMessenger"
#define MESSENGER_DESTROY_METHOD_SIGNATURE "(Ljava/lang/String;)V"

#define MESSENGER_INTERFACE_CLASS "r2d2/msg/Messenger"
#define MSG_DISPATCH_METHOD "dispatch"
#define MSG_DISPATCH_METHOD_SIGNATURE "(Ljava/lang/String;)V"


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

static st_r2d2_share *get_share(const char *table_name, TABLE * table);
static void free_share(st_r2d2_share *share);

/*****************************************************************************
** r2d2 tables
*****************************************************************************/

//system variables for the engine
static char*  srv_jvm_arguments_var  = ""; //has 2b set 2 something like "-Djava.class.path=/Users/vishnuhr/volume/mysql-r2d2/r2d2-java/r2d2-java/target/classes"
//static reference to jvm env object which is created in init Method
static JNIEnv* env = NULL;


ha_r2d2::ha_r2d2(handlerton *hton,
                           TABLE_SHARE *table_arg)
  :handler(hton, table_arg)
{
    buffer.set((char*)byte_buffer, IO_SIZE, &my_charset_bin);  //this buffer holds the message to be published.
}


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

  if (!(share= get_share(name, table)))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  thr_lock_data_init(&share->lock, &lock, NULL);
  DBUG_RETURN(0);
}

int ha_r2d2::close(void)
{
  DBUG_ENTER("ha_r2d2::close");
  JavaVM* java_vm;

    env->GetJavaVM(&java_vm);

    if(java_vm->AttachCurrentThread((void **)&env, NULL) < 0)
    {
          fprintf(stderr,"[R2D2] Could not close table. Thread attachement to jvm env failed\n");
          DBUG_RETURN(-1);
    }

  char * className = FACTORY_CLASS;
  char * staticMethodName = MESSENGER_DESTROY_METHOD;
  char * staticMethodSignature = MESSENGER_DESTROY_METHOD_SIGNATURE ;

  jclass factory_class = load_class(env, className);

  if(factory_class == NULL) {
    fprintf(stderr, "[R2D2] TABLE CLOSE: %s - cant load factory class to destroy messenger. could not load class %s! ",table->s->table_name.str, className);
    DBUG_RETURN(-1);
  }

  jmethodID staticFactoryMethod = get_static_method(env,factory_class ,className ,staticMethodName,staticMethodSignature  );
  if(staticFactoryMethod == NULL) {
    fprintf(stderr, "[R2D2] TABLE CLOSE: %s - cant load factory destroy method to destroy messenger. could not load method %s of class %s! ", table->s->table_name.str,staticMethodName,className );
    DBUG_RETURN(-1);
  }

  fprintf(stderr,"[R2D2] TABLE CLOSE: %s - going to close messenger resources....!\n", table->s->table_name);
  if(call_messenger_factory_destroy_method(env ,staticMethodName ,factory_class, staticFactoryMethod , table->s->connect_string.str) < 0){
    fprintf(stderr,"[R2D2] TABLE CLOSE: %s - Tried to close messenger resources. but error occured!", table->s->table_name);
  }

  if(java_vm->DetachCurrentThread() < 0 )//thread done detached it with ok
    {
       fprintf(stderr,"[R2D2] Closing table.....Thread DeAttachement to jvm env failed....\n");
       //DBUG_RETURN(-1); //dont return -1 code. if invoke on java obj succeeds. then dont return -1 else return -1
    }
  free_share(share);
  fprintf(stderr,"[R2D2] TABLE CLOSE: %s - done closing messenger resources!\n", table->s->table_name);
  DBUG_RETURN(0);
}

int ha_r2d2::create(const char *name, TABLE *table_arg,
                         HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("ha_r2d2::create");
  fprintf(stderr,"[R2D2] creating table with configuration(config passed via connect string): %s\n",table_arg->s->connect_string.str);

  if(table_arg->s->connect_string.str == NULL || strlen(table_arg->s->connect_string.str) == 0){
      fprintf(stderr,"[R2D2] NULL/Empty configuration passed(config passed via connect string): \n");
      DBUG_RETURN(-1);

  }
  bool is_payload_colum_present = false;
  for (Field **field=table_arg->field ; *field ; field++)
  {
        if(strcmp(MSG_PAYLOAD_COLUMN,(*field)->field_name ) == 0){
           is_payload_colum_present = true;
        }
  }
  if(!is_payload_colum_present){
     fprintf(stderr,"[R2D2] Mandatory column '%s' for table: %s not found in create statement.\n",MSG_PAYLOAD_COLUMN,table_arg->s->table_name);
     DBUG_RETURN(-1);
  }

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

void ha_r2d2::retrieve_message_body(uchar * buf){

    char attribute_buffer[MAX_MSG_SIZE];//10kb message max.
    String attribute(attribute_buffer, sizeof(attribute_buffer), &my_charset_bin);

    buffer.length(0); //reset it.

    bool is_payload_colum_present = false;

    //get the only column i.e body column
    for (Field **field=table->field ; *field ; field++)
    {
            //fprintf(stderr,"creating field of table: %s\n",(*field)->field_name);
            if(strcmp(MSG_PAYLOAD_COLUMN,(*field)->field_name ) == 0){
               (*field)->val_str(&attribute,&attribute);
            }
    }
    //Field **body_column=table->field;
    //(*body_column)->val_str(&attribute,&attribute);

    buffer.append(attribute);

    if(buffer.length() > MAX_MSG_SIZE){
        fprintf(stderr,"[Warning] Table %s with R2D2 storage engine recevied record with length >  %d. truncating data!",table->s->table_name.str, MAX_MSG_SIZE);
    }
}


int ha_r2d2::write_row(uchar * buf)
{
  DBUG_ENTER("ha_r2d2::write_row");

  if (!(share= get_share(table->s->table_name.str, table)))
        DBUG_RETURN(HA_ERR_OUT_OF_MEM);

  JavaVM* java_vm;

  env->GetJavaVM(&java_vm);

  if(java_vm->AttachCurrentThread((void **)&env, NULL) < 0)
  {
        fprintf(stderr,"[R2D2] Could not write row. Thread attachement to jvm env failed\n");
        DBUG_RETURN(-1);
  }
  char * topic = table->s->table_name.str;
  char * config = table->s->connect_string.str;

  char * className = FACTORY_CLASS;
  char * staticMethodName = MESSENGER_GETTER_METHOD;
  char * interface_class_name = MESSENGER_INTERFACE_CLASS;
  char * objectMethodName = MSG_DISPATCH_METHOD ;

  char * staticMethodSignature = MESSENGER_GETTER_METHOD_SIGNATURE ;
  char * objectMethodSignature = MSG_DISPATCH_METHOD_SIGNATURE;

  jclass factory_class;
  jclass interface_class;
  jmethodID staticFactoryMethod;
  jmethodID interfaceMethod;

  retrieve_message_body(buf);

  factory_class = load_class(env, className);
  if(factory_class == NULL) { fprintf(stderr, "[R2D2]cant initialize R2D2 engine. could not load class %s! ", className); return ENGINE_INIT_FAILED;}

  staticFactoryMethod = get_static_method(env,factory_class ,className ,staticMethodName,staticMethodSignature  );
  if(staticFactoryMethod == NULL) { fprintf(stderr, "[R2D2]cant initialize R2D2 engine. could not load method %s of class %s! ", staticMethodName,className ); return ENGINE_INIT_FAILED;}

  interface_class = load_class(env, interface_class_name);
  if(interface_class == NULL) { fprintf(stderr, "[R2D2]cant initialize R2D2 engine. could not load class %s! ", interface_class_name); return ENGINE_INIT_FAILED;}

  interfaceMethod = get_object_method(env, interface_class ,interface_class_name ,objectMethodName,  objectMethodSignature);
  if(interfaceMethod == NULL) { fprintf(stderr, "[R2D2]cant initialize R2D2 engine. could not load method %s of class %s! ", objectMethodName,interface_class_name ); return ENGINE_INIT_FAILED;}

  jobject messenger_object = call_messenger_factory_create_method(env ,staticMethodName ,factory_class, staticFactoryMethod , config);

  if(messenger_object == NULL){
     fprintf(stderr,"Failed to write_row for R2D2 table: %s, could not get instance of messenger object !!! \n",topic);
     DBUG_RETURN(-1);
  }

  if(invoke_msg_dispatch(env, messenger_object , interfaceMethod, buffer.c_ptr(), topic ) < 0){
     fprintf(stderr,"Failed to write_row for R2D2 table: %s, could not invoke method on messenger object !!! \n",topic);
     DBUG_RETURN(-1);
  }

  if(java_vm->DetachCurrentThread() < 0 )//thread done detached it with ok
  {
     fprintf(stderr,"[R2D2] Writing row.....Thread DeAttachement to jvm env failed....\n");
     //DBUG_RETURN(-1); //dont return -1 code. if invoke on java obj succeeds. then dont return -1 else return -1
  }

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


static st_r2d2_share *get_share(const char *table_name, TABLE* table)
{
  st_r2d2_share *share;
  uint length;

  length= (uint) strlen(table_name);
  mysql_mutex_lock(&r2d2_mutex);   //take mutex lock
    
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
  r2d2_hton->db_type= DB_TYPE_BLACKHOLE_DB;  //todo:change.. not too important for now
  r2d2_hton->create= r2d2_create_handler;
  r2d2_hton->flags= HTON_CAN_RECREATE;

  mysql_mutex_init(bh_key_mutex_r2d2,
                   &r2d2_mutex, MY_MUTEX_INIT_FAST);
  (void) my_hash_init(&r2d2_open_tables, system_charset_info,32,0,0,
                      (my_hash_get_key) r2d2_get_key,
                      (my_hash_free_key) r2d2_free_key, 0);

  fprintf(stderr, "[R2D2] Doing initialization of R2D2 storage engine...\n");
  fprintf(stderr, "[R2D2] r2d2_jvm_arguments_var set to '%s'\n",srv_jvm_arguments_var);

  //check empty option
  if(srv_jvm_arguments_var == NULL || srv_jvm_arguments_var[0] == '\0' || strlen(srv_jvm_arguments_var) == 0)
  {
    fprintf(stderr, "[R2D2]cant initialize R2D2 engine. variable 'r2d2_jvm_arguments_var' is empty/null in your cnf ! ");
    return ENGINE_INIT_FAILED;
  }

  //init the jvm using jvm options. //no need for mutex over this vm creation. as init is called only once for registering the engine.
  env = create_vm(srv_jvm_arguments_var);
  //

  if(env == NULL)
  {
      fprintf(stderr, "[R2D2]cant initialize R2D2 engine. JVM creation failed! ");
      return ENGINE_INIT_FAILED;
  }

  JavaVM* java_vm;

  env->GetJavaVM(&java_vm);

  if(java_vm->AttachCurrentThread((void **)&env, NULL) < 0)
  {
    fprintf(stderr,"[R2D2] Could not init engine. Thread attachement to jvm env failed\n");
    DBUG_RETURN(-1);
  }

  char * className = FACTORY_CLASS;
  jclass factory_class = load_class(env, className);

  if(factory_class == NULL) {
      //check and see if engine can load messenger factory class.
      fprintf(stderr, "[R2D2] Warning: Could not load factory class: %s! check the r2d2_jvm_arguments_var arg for classpath! & restart server\n", className);
      fprintf(stderr, "[R2D2] This warning is on purpose. This has been done for the initial registration of the storage engine to succeed while doing import `INSTALL PLUGIN r2d2 SONAME 'ha_r2d2.so';`\n");
      //the above warning is logged on purpose for user to set the "r2d2_jvm_arguments_var properly". initially its empty
  }
  fprintf(stderr, "[R2D2]Done with initialization of R2D2 storage engine.\n");

  if(java_vm->DetachCurrentThread() < 0 )//thread done detached it with ok
  {
       fprintf(stderr,"[R2D2] Init Engine.....Thread DeAttachement to jvm env failed....\n");
       //DBUG_RETURN(-1); //dont return -1 code. if invoke on java obj succeeds. then dont return -1 else return -1
  }
  return ENGINE_INIT_SUCCESS;
}

static int r2d2_fini(void *p)
{
  my_hash_free(&r2d2_open_tables);
  mysql_mutex_destroy(&r2d2_mutex);

  //destroy the vm. but destroying does not work due to bug.
  //http://stackoverflow.com/questions/6149930/create-jvm-after-destroying-it
  //DESTROY THE JVM .. but currently destroying the vm causes YOUR PROCESS to HANG
  // http://www.velocityreviews.com/forums/t141022-destroyjavavm-never-returns.html

  return 0;
}

struct st_mysql_storage_engine r2d2_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

///// system vars for engine////

static MYSQL_SYSVAR_STR(
   jvm_arguments_var, //name of the variable i.e. access it like this r2d2_jvm_arguments_var="-DJVM..."
  srv_jvm_arguments_var,   // internal used variable defined above.
  PLUGIN_VAR_READONLY,
  "jvm arguments for the r2d2 engine to load producer classes.",
  NULL,
  NULL,
  "-Dsome_jvm_system_variable=foobar");//setting default value of r2d2_jvm_arguments_var so init of engine passes when registration happens first time

static struct st_mysql_sys_var* r2d2_system_variables[]= {
  MYSQL_SYSVAR(jvm_arguments_var),
  NULL
};


////////////////////////////////

mysql_declare_plugin(r2d2)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &r2d2_storage_engine,
  "r2d2",
  "vishnu rao",
  "Interface with message queues.(may the force be with u)",
  PLUGIN_LICENSE_GPL,
  r2d2_init, /* Plugin Init */
  r2d2_fini, /* Plugin Deinit */
  0x0100 /* 1.0 */,
  NULL,                       /* status variables                */
  r2d2_system_variables,                       /* system variables                */
  NULL,                       /* config options                  */
  0,                          /* flags                           */
}
mysql_declare_plugin_end;

/*
  test cases
  ----------
1) empty/null srv_jvm_arguments_var paramter -> registration of engine to fail
2) bad value of srv_jvm_arguments_var,  registration of engine to fail.
http://stackoverflow.com/questions/2093112/why-i-should-not-reuse-a-jclass-and-or-jmethodid-in-jni
*/

/*
todo:
1) exception handling in c++ for jni methods.
2) have ping class for testing class loading.
3) counter number of messages.
4) per table have producer.
5) copyright not oracle :) !!!!
6) call stop method in jni to stop subscribers.
7) dereference the strings in jni for garbage collection
8)-Xcheck:jni
*/
