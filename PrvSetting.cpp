#include "PrvSetting.h"

/* 共用体宣言 */
_mDATA mdata[12];
_FLAG  flag;
_FLAG  err_flag;

/* 平日且つ祝日のデータ生成 */
void set_daydata( void )
{
  /* 初期化(bit32はデバッグ用で1) */
  mdata[0].all_bits  = 0x80000000;
  mdata[1].all_bits  = 0x80000000;
  mdata[2].all_bits  = 0x80000000;
  mdata[3].all_bits  = 0x80000000;
  mdata[4].all_bits  = 0x80000000;
  mdata[5].all_bits  = 0x80000000;
  mdata[6].all_bits  = 0x80000000;
  mdata[7].all_bits  = 0x80000000;
  mdata[8].all_bits  = 0x80000000;
  mdata[9].all_bits  = 0x80000000;
  mdata[10].all_bits = 0x80000000;
  mdata[11].all_bits = 0x80000000;

  /* 平日且つ祝日の日にちを設定 */
  mdata[0].d.bit01 = 1; /* 例：1月1日 */
}

/* 日時のビットフィールドから任意の日付の値を取得 */
uint8_t get_daydata( uint8_t month, uint8_t day )
{
  uint8_t res_data;
  
  res_data = ( mdata[ month - 1 ].all_bits >> ( day - 1 ) ) & 1;
  
  return res_data;
}

/* 汎用フラグ初期化関数 */
void flag_init( void )
{
  flag.all_bits = 0x00;
  err_flag.all_bits = 0x00;
  
  flag_adjustsync = 1;
}
