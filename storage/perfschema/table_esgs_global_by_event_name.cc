/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA */

/**
  @file storage/perfschema/table_esgs_global_by_event_name.cc
  Table EVENTS_STAGES_SUMMARY_GLOBAL_BY_EVENT_NAME (implementation).
*/

#include "my_global.h"
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_esgs_global_by_event_name.h"
#include "pfs_global.h"
#include "pfs_instr.h"
#include "pfs_timer.h"
#include "pfs_visitor.h"
#include "field.h"

THR_LOCK table_esgs_global_by_event_name::m_table_lock;

PFS_engine_table_share_state
table_esgs_global_by_event_name::m_share_state = {
  false /* m_checked */
};

PFS_engine_table_share
table_esgs_global_by_event_name::m_share=
{
  { C_STRING_WITH_LEN("events_stages_summary_global_by_event_name") },
  &pfs_truncatable_acl,
  table_esgs_global_by_event_name::create,
  NULL, /* write_row */
  table_esgs_global_by_event_name::delete_all_rows,
  table_esgs_global_by_event_name::get_row_count,
  sizeof(PFS_simple_index),
  &m_table_lock,
  { C_STRING_WITH_LEN("CREATE TABLE events_stages_summary_global_by_event_name("
                      "EVENT_NAME VARCHAR(128) not null comment 'Event name.',"
                      "COUNT_STAR BIGINT unsigned not null comment 'Number of summarized events, which includes all timed and untimed events.',"
                      "SUM_TIMER_WAIT BIGINT unsigned not null comment 'Total wait time of the timed summarized events.',"
                      "MIN_TIMER_WAIT BIGINT unsigned not null comment 'Minimum wait time of the timed summarized events.',"
                      "AVG_TIMER_WAIT BIGINT unsigned not null comment 'Average wait time of the timed summarized events.',"
                      "MAX_TIMER_WAIT BIGINT unsigned not null comment 'Maximum wait time of the timed summarized events.')") },
  false, /* m_perpetual */
  false, /* m_optional */
  &m_share_state
};

PFS_engine_table*
table_esgs_global_by_event_name::create(void)
{
  return new table_esgs_global_by_event_name();
}

int
table_esgs_global_by_event_name::delete_all_rows(void)
{
  reset_events_stages_by_thread();
  reset_events_stages_by_account();
  reset_events_stages_by_user();
  reset_events_stages_by_host();
  reset_events_stages_global();
  return 0;
}

ha_rows
table_esgs_global_by_event_name::get_row_count(void)
{
  return stage_class_max;
}

table_esgs_global_by_event_name::table_esgs_global_by_event_name()
  : PFS_engine_table(&m_share, &m_pos),
    m_row_exists(false), m_pos(1), m_next_pos(1)
{
  m_normalizer= time_normalizer::get_stage();
}

void table_esgs_global_by_event_name::reset_position(void)
{
  m_pos= 1;
  m_next_pos= 1;
}

int table_esgs_global_by_event_name::rnd_init(bool scan)
{
  return 0;
}

int table_esgs_global_by_event_name::rnd_next(void)
{
  PFS_stage_class *stage_class;

  if (global_instr_class_stages_array == NULL)
    return HA_ERR_END_OF_FILE;

  m_pos.set_at(&m_next_pos);

  stage_class= find_stage_class(m_pos.m_index);
  if (stage_class)
  {
    make_row(stage_class);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int
table_esgs_global_by_event_name::rnd_pos(const void *pos)
{
  PFS_stage_class *stage_class;

  set_position(pos);

  if (global_instr_class_stages_array == NULL)
    return HA_ERR_END_OF_FILE;

  stage_class=find_stage_class(m_pos.m_index);
  if (stage_class)
  {
    make_row(stage_class);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}


void table_esgs_global_by_event_name
::make_row(PFS_stage_class *klass)
{
  m_row.m_event_name.make_row(klass);

  PFS_connection_stage_visitor visitor(klass);
  PFS_connection_iterator::visit_global(true,  /* hosts */
                                        false, /* users */
                                        true,  /* accounts */
                                        true,  /* threads */
                                        false, /* THDs */
                                        & visitor);

  m_row.m_stat.set(m_normalizer, & visitor.m_stat);
  m_row_exists= true;
}

int table_esgs_global_by_event_name
::read_row_values(TABLE *table, unsigned char *, Field **fields,
                  bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* NAME */
        m_row.m_event_name.set_field(f);
        break;
      default: /* 1, ... COUNT/SUM/MIN/AVG/MAX */
        m_row.m_stat.set_field(f->field_index - 1, f);
        break;
      }
    }
  }

  return 0;
}

