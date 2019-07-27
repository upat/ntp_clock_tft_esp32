#ifndef _PRVSETTING_H
#define _PRVSETTING_H

#include <Arduino.h>

typedef struct flag_bits {
  uint8_t bit01 : 1;
  uint8_t bit02 : 1;
  uint8_t bit03 : 1;
  uint8_t bit04 : 1;
  uint8_t bit05 : 1;
  uint8_t bit06 : 1;
  uint8_t bit07 : 1;
  uint8_t bit08 : 1;
} _FB;

/* 12ヶ月分のデータを生成 */
typedef union {
  uint32_t all_bits;
  
  /* 1日～31日までのビットフィールド */
  struct {
    uint32_t bit01 : 1;
    uint32_t bit02 : 1;
    uint32_t bit03 : 1;
    uint32_t bit04 : 1;
    uint32_t bit05 : 1;
    uint32_t bit06 : 1;
    uint32_t bit07 : 1;
    uint32_t bit08 : 1;
    uint32_t bit09 : 1;
    uint32_t bit10 : 1;
    uint32_t bit11 : 1;
    uint32_t bit12 : 1;
    uint32_t bit13 : 1;
    uint32_t bit14 : 1;
    uint32_t bit15 : 1;
    uint32_t bit16 : 1;
    uint32_t bit17 : 1;
    uint32_t bit18 : 1;
    uint32_t bit19 : 1;
    uint32_t bit20 : 1;
    uint32_t bit21 : 1;
    uint32_t bit22 : 1;
    uint32_t bit23 : 1;
    uint32_t bit24 : 1;
    uint32_t bit25 : 1;
    uint32_t bit26 : 1;
    uint32_t bit27 : 1;
    uint32_t bit28 : 1;
    uint32_t bit29 : 1;
    uint32_t bit30 : 1;
    uint32_t bit31 : 1;
    uint32_t bit32 : 1; /* Reserve */
  } d;
} _mDATA;

/* フラグ用共用体 */
typedef union {
  uint8_t all_bits;
  _FB fb;
} _FLAG;

/* 変数定義 */
extern _mDATA mdata[12];
extern _FLAG  flag;
extern _FLAG  err_flag;

#define flag_holidayjdg ( flag.fb.bit01 ) /* 平日且つ祝日の判定に使用するフラグ */
#define flag_adjustsync ( flag.fb.bit02 ) /* 初回起動時に0分0秒でNTP更新周期を再設定するフラグ */
#define flag_hour_digit ( flag.fb.bit03 ) /* 時間表示時に時間の桁数によってカーソル位置の調整に使用するフラグ */

#define flag_wifiinit_err ( err_flag.fb.bit08 ) /* wifi接続エラー判定フラグ */
#define flag_udpbegin_err ( err_flag.fb.bit07 ) /* udp接続エラー判定フラグ */

/* 定数定義 */
#define AP_SSID "" /* 接続するルーターのSSID */
#define AP_PASS "" /* 接続するルーターのパスワード */

#define TIME_SERVER    "ntp.nict.jp" /* NTPサーバーのドメイン名(IP指定は非推奨) */
#define NTP_PORT       5000          /* NTPサーバーとのUDP通信ポート */
#define DATA_RCV_PORT  9200          /* ローカルサーバーとのUDP通信ポート */
#define TIME_ZONE      9             /* タイムゾーンの設定(日本なら9) */
#define UPDATE_MIN_PRE 59            /* 1分経過直前の秒の値 */

#define SCREEN_WIDTH  320 /* TFTの横のピクセル数 */
#define SCREEN_HEIGHT 240 /* TFTの縦のピクセル数 */
#define TFT_RST       16  /* TFTのリセット端子(無い場合は0) */

#define HTTP_URL "" /* HTTPサーバー用(test) */

#define UDP_DEFAULT   "----.-hPa --% --.-" /* UDP受信データ初期値 */
#define DAY_FORMAT    "%04d/%02d/%02d"     /* 年月日フォーマット */
#define TIME_FORMAT   "%d:%02d:%02d"       /* 時刻フォーマット */
#define SENSOR_FORMAT "%2.0f%% %2.0f"      /* 温湿度フォーマット */

#define SERIAL_ESP8266 74880  /* ESP8266用シリアル通信ビットレート */
#define SERIAL_ESP32   115200 /* ESP32用シリアル通信ビットレート */

/* 関数定義 */
extern void    set_daydata( void );
extern uint8_t get_daydata( uint8_t month, uint8_t day );
extern void    flag_init( void );

#endif
