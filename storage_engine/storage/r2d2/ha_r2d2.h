/* Copyright (c) /* Copyright (c) todo */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "thr_lock.h"                           /* THR_LOCK */
#include "handler.h"                            /* handler */
#include "table.h"                              /* TABLE_SHARE */
#include "myjni.h"

/*
  Shared structure for correct LOCK operation
*/
struct st_r2d2_share {
  THR_LOCK lock;
  uint use_count;
  uint table_name_length;
  char table_name[1];
};

#define R2D2_QUERY_BUFFER_SIZE STRING_BUFFER_USUAL_SIZE * 5
/*
  Class definition for the r2d2 storage engine
  "may the force be with you !"
*/
class ha_r2d2: public handler
{
  THR_LOCK_DATA lock;      /* MySQL lock */
  st_r2d2_share *share;
  uchar byte_buffer[IO_SIZE];
  String buffer;

public:
  ha_r2d2(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_r2d2()
  {
  }
  /* The name that will be used for display purposes */
  const char *table_type() const { return "R2D2"; }
  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
  */
  const char *index_type(uint key_number);
  const char **bas_ext() const;
  ulonglong table_flags() const
  {
    return(HA_NULL_IN_KEY | HA_CAN_FULLTEXT | HA_CAN_SQL_HANDLER |
           HA_BINLOG_STMT_CAPABLE | HA_BINLOG_ROW_CAPABLE |
           HA_CAN_INDEX_BLOBS | HA_AUTO_PART_KEY |
           HA_FILE_BASED | HA_CAN_GEOMETRY | HA_CAN_INSERT_DELAYED);
  }
  ulong index_flags(uint inx, uint part, bool all_parts) const
  {
    return ((table_share->key_info[inx].algorithm == HA_KEY_ALG_FULLTEXT) ?
            0 : HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE |
            HA_READ_ORDER | HA_KEYREAD_ONLY);
  }
  /* The following defines can be increased if necessary */
#define R2D2_MAX_KEY	64		/* Max allowed keys */
#define R2D2_MAX_KEY	64		/* Max allowed keys */
#define R2D2_MAX_KEY_LENGTH 1000
  uint max_supported_keys()          const { return R2D2_MAX_KEY; }
  uint max_supported_key_length()    const { return R2D2_MAX_KEY_LENGTH; }
  uint max_supported_key_part_length() const { return R2D2_MAX_KEY_LENGTH; }
  int open(const char *name, int mode, uint test_if_locked);
  int close(void);
  int truncate();
  int rnd_init(bool scan);
  int rnd_next(uchar *buf);
  int rnd_pos(uchar * buf, uchar *pos);
  int index_read_map(uchar * buf, const uchar * key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);
  int index_read_idx_map(uchar * buf, uint idx, const uchar * key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag);
  int index_read_last_map(uchar * buf, const uchar * key, key_part_map keypart_map);
  int index_next(uchar * buf);
  int index_prev(uchar * buf);
  int index_first(uchar * buf);
  int index_last(uchar * buf);
  void position(const uchar *record);
  int info(uint flag);
  int external_lock(THD *thd, int lock_type);
  int create(const char *name, TABLE *table_arg,
             HA_CREATE_INFO *create_info);
  THR_LOCK_DATA **store_lock(THD *thd,
                             THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
private:
  virtual int write_row(uchar *buf);
  virtual int update_row(const uchar *old_data, uchar *new_data);
  virtual int delete_row(const uchar *buf);
  virtual void retrieve_message_body(uchar * buf);
};
