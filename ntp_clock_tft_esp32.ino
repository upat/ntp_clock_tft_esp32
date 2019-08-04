#include "NTP_Clock_Tools.h"

#include <SPI.h>
#include <DHT.h>
#include <TimeLib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

Adafruit_ILI9341 tft = Adafruit_ILI9341( 5, 17, 16 ); /* CS, DC, RESET */
DHT dht( 33, DHT11 );

/* 温湿度データ */
float humi = 0.0;
float temp = 0.0;
/* 現在時刻(日本時間)を取得 */
time_t now_data = 0;
/* UDP通信用 */
char udp_buff[24] = {};
/* テキスト色設定 */
uint16_t text_color = ILI9341_ORANGE;

void     set_display( uint8_t h_data, uint8_t m_data, uint8_t s_data );
void     read_sensor( void );
void     adjust_syncinterval( void );
void     tft_wake( void );
void     tft_sleep( void );
void     deepsleep_jdg( uint8_t h_data, uint8_t w_data );
time_t   getNtpTime( void );

void setup()
{
  /* 平日且つ祝日のデータ生成 */
  set_daydata();
  /* フラグ初期化 */
  flag_init();

  /* TFTバックライトのPWM出力 */
  ledcSetup( 0, 12800, 8 ); /* ch0のPWM設定 */
  ledcAttachPin( A4, 0 );   /* IO32にch0を割り当て */
  ledcWrite( 0, 128 );      /* ch0からPWM出力 */

  Serial.begin( SERIAL_SPEED );
  
  /* 温湿度センサーの開始 */
  dht.begin();

  /* wi-fi通信の開始 */
  flag_wifiinit_err = wifi_init();

  /* UDP通信の開始 */
  if( !UDP_NTP.begin( NTP_PORT ) || !UDP_RCV.begin( data_rcv_port[0] ) )
  {
    /* 失敗時 */
    flag_udpbegin_err = 1;
  }

  /* TFT制御の開始 */
  tft.begin();
  tft.setRotation( 1 );
  tft.setTextColor( text_color, ILI9341_BLACK ); /* 表示する文字色(同じ場所へ表示するので背景色も設定) */

  /* エラー判定処理 */
  if( 0 < err_flag.all_bits )
  {
    /* 初回起動時のwifi接続に失敗するのでリセットがかかる */
    
    /* TFTが動作している場合 */
    tft.fillScreen( ILI9341_BLACK );       /* 画面表示のクリア */
    tft.setTextSize( 3 );                  /* 表示する文字サイズ */
    tft.setCursor( 0, 0 );                 /* 文字描画の開始位置 */
    tft.print( "init_error:0x" );          /* 文字のデータをセット */
    tft.println( err_flag.all_bits, HEX );

    /* 処理を開始せずソフトウェアリセット */
    delay( 1000 );
    ESP.restart();
    delay( 5000 ); /* 未到達 */
  }
  else
  {
    setSyncProvider( getNtpTime ); /* 補正に使用する関数設定 */
    setSyncInterval( 3600 );       /* 時刻補正を行う周期設定(秒) 後で調整 */

    /* 平日の祝日か */
    flag_holidayjdg = get_daydata( ( uint8_t )month( now() ), day( now() ) );

    /* 温湿度データの取得(初回) */
    read_sensor();

    /* スリープ判定処理 */
    deepsleep_jdg( hour( now() ), weekday( now() ) );

    sprintf( udp_buff, "%s", UDP_DEFAULT );
    post_req( "weather" );
  }
}

void loop()
{
  static uint8_t w_data = 1; /* 曜日 */
  static uint8_t d_data = 0; /* 日   */
  static uint8_t h_data = 0; /* 時   */
  static uint8_t m_data = 0; /* 分   */
  static uint8_t s_data = 0; /* 秒   */
  
  /* NTPから取得した時刻が設定済み且つ時刻が更新された時 */
  if( timeNotSet != timeStatus() && now_data != now() )
  {
    now_data = now();
    h_data = hour( now_data );
    m_data = minute( now_data );
    s_data = second( now_data );

    /* スリープ判定処理 */
    deepsleep_jdg( h_data, w_data );

    /* 画面表示 */
    set_display( h_data, m_data, s_data );

    /* 1分ごとに温湿度更新 */
    if( UPDATE_MIN_PRE == s_data )
    {
      read_sensor();
    }

    /* 毎時10分にUDPからデータ取得準備 */
    if( 10 == m_data && 0 == s_data )
    {
      sprintf( udp_buff, "%s", UDP_DEFAULT );
    }

    /* 日付が変わっていたら平日の祝日か判定 */
    if( d_data != day( now_data ) )
    {
      w_data = weekday( now_data );
      d_data = day( now_data );
      flag_holidayjdg = get_daydata( ( uint8_t )month( now_data ), d_data );
    }

    /* UDP受信確認 */
    udp_rcv( udp_buff );
  }

  delay( 100 );
}

/* TFTに描画する関数 */
void set_display( uint8_t h_data, uint8_t m_data, uint8_t s_data )
{
  /* 前回値 */
  static uint8_t day_pre = 0;
  static uint8_t min_pre = 0;
  static uint8_t udpstr_pre = 0;
  static uint8_t ssrstr_pre = 0;

  /* 表示文字列を格納する配列 */
  char day_data[12] = {};
  char time_data[12] = {};
  char sensor_data[12] = {};

  /* 画面上の文字の長さ */
  uint16_t udp_strlen = 0;
  uint16_t sensor_strlen = 0;
  /* 中央寄せに使用するカーソル開始位置 */
  uint16_t udp_cursor = 0;
  uint16_t sensor_cursor = 0;

  /* 日時の表示 */
  if( day_pre != day( now_data ) )
  {
    /* 表示文字列の作成(年月日) */
    sprintf( day_data, DAY_FORMAT, year( now_data ), month( now_data ), day( now_data ) );

    tft.fillScreen( ILI9341_BLACK ); /* 画面表示クリア */
    
    /* 日付のデータをセット */
    tft.setTextSize( 3 );
    tft.setCursor( 27, 24 );
    tft.println( day_data );
  
    /* 曜日のデータをセット */
    tft.setCursor( 207, 24 );
    tft.println( week_day[weekday( now_data ) - 1] );
  }

  /* 温度の表示 */
  if( min_pre != m_data )
  {
    /* 時間表示の桁数調整 */
    if( 10 > h_data )
    {
      flag_hour_digit = 1;
    }
    else
    {
      flag_hour_digit = 0;
    }
    
    /* 表示文字列の作成(温湿度) */
    sprintf( sensor_data, SENSOR_FORMAT, humi, temp );

    /* 画面上の文字の長さ */
    udp_strlen = count_char( udp_buff, sizeof( udp_buff ) ) * 12;
    sensor_strlen = count_char( sensor_data, sizeof( sensor_data ) ) * 18;
    /* 中央寄せに使用するカーソル開始位置 */
    udp_cursor = str_position( 320, udp_strlen, 10 );
    sensor_cursor = str_position( 320, sensor_strlen, 18 );

    /* 文字数が異なる場合のリフレッシュ */
    if( ( udpstr_pre != udp_strlen ) || ( ssrstr_pre != sensor_strlen ) )
    {
      tft.fillRect( 0, 70, 320, 50, ILI9341_BLACK ); /* 温度情報の画面表示クリア */
      udpstr_pre = udp_strlen;                       /* UDP受信文字数の前回値更新 */
      ssrstr_pre = sensor_strlen;                    /* センサー読み取り文字数の前回値更新 */
    }
    
    /* 気温のデータをセット(文字サイズ:2) */
    tft.setTextSize( 2 );
    tft.setCursor( udp_cursor, 70 );
    tft.println( udp_buff );
  
    /* 温度単位をそれっぽく表示(文字サイズ:2) */
    tft.setCursor( udp_cursor + udp_strlen + 6, 70 );
    tft.println( "C" );
    tft.fillRect( udp_cursor + udp_strlen, 70, 4, 4, text_color );
  
    /* 温湿度のデータをセット(文字サイズ:3) */
    tft.setTextSize( 3 );
    tft.setCursor( sensor_cursor, 96 );
    tft.println( sensor_data );
  
    /* 温度単位をそれっぽく表示(文字サイズ:3) */
    tft.setCursor( sensor_cursor + sensor_strlen + 8, 96 );
    tft.println( "C" );
    tft.fillRect( sensor_cursor + sensor_strlen, 96, 6, 6, text_color );
  }

  /* 表示文字列の作成(時間) */
  sprintf( time_data, TIME_FORMAT, h_data, m_data, s_data );

  /* 2桁時間→1桁時間のリフレッシュは日時変更時に行っている */
  /* 時間のデータをセット(文字サイズ:6) */
  tft.setTextSize( 6 );
  tft.setCursor( 16 + ( flag_hour_digit * 16 ), 160 );
  tft.println( time_data );

  /* NTP取得時間の調整(一度だけ) */
  adjust_syncinterval();

  /* 前回値更新 */
  day_pre = day( now_data );
  min_pre = m_data;
}

/* 温湿度データ取得関数の呼び出し */
void read_sensor( void )
{
  humi = dht.readHumidity();
  temp = dht.readTemperature();
}

/* 一度だけNTP取得時間の調整 */
void adjust_syncinterval( void )
{
  /* flag_adjustsyncの初期値は1 */
  if( flag_adjustsync && 0 == minute() && 0 == second() )
  {
    setSyncInterval( 21600 ); /* 時刻補正を行う周期設定(秒) */
    
    flag_adjustsync = 0;
    Serial.println( "Adjust SyncInterval" );
  }
}

/* TFTウェイク処理 */
void tft_wake( void )
{
  tft.startWrite();
  
  tft.writeCommand( ILI9341_SLPOUT );
  delay( 120 );

  tft.writeCommand( ILI9341_DISPON );
  delay( 20 );

  tft.endWrite();
}

/* TFTスリープ処理 */
void tft_sleep( void )
{
  tft.startWrite();

  tft.writeCommand( ILI9341_DISPOFF );
  delay( 20 );

  tft.writeCommand( ILI9341_SLPIN );
  delay( 120 );

  tft.endWrite();
}

/* 平日の日中にdeep-sleepを行う判定処理 */
void deepsleep_jdg( uint8_t h_data, uint8_t w_data )
{
  if(
    8 < h_data &&
    19 > h_data &&
    1 < w_data &&
    7 > w_data &&
    !flag_holidayjdg
  )
  //if( 38 == m_data && 0 == s_data && !public_holiday )
  {
    /* サーバー停止処理 */
    post_req( String( ( int )temp ) + "℃"  ); /* スリープ時の室温 */
    if( 31 < ( int )temp )
    {
      post_req( "temp_alert" ); /* サーバー停止要求 */
    }
    
    /* 画面のスリープ処理 */
    tft.fillScreen( ILI9341_BLACK );
    tft_sleep();
    ledcWrite( 0, 0 ); /* バックライト消灯 */
    
    /* deep-sleep */
    esp_sleep_enable_timer_wakeup( ( uint32_t )( 1800 * 1000 * 1000 ) ); 
    esp_deep_sleep_start();
    delay( 3000 ); /* 未到達 */
  }
}

/*** Timeライブラリのサンプルコードから移植 ***/
time_t getNtpTime()
{
  while ( UDP_NTP.parsePacket() > 0 ) ; // discard any previously received packets
  // Serial.println( "Transmit NTP Request" );
  sendNTPpacket( TIME_SERVER );
  uint32_t beginWait = millis();
  while ( millis() - beginWait < 1500 ) {
    int size = UDP_NTP.parsePacket();
    if ( size >= NTP_PACKET_SIZE ) {
      // Serial.println( "Receive NTP Response" );
      UDP_NTP.read( packetBuffer, NTP_PACKET_SIZE );  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  ( unsigned long )packetBuffer[40] << 24;
      secsSince1900 |= ( unsigned long )packetBuffer[41] << 16;
      secsSince1900 |= ( unsigned long )packetBuffer[42] << 8;
      secsSince1900 |= ( unsigned long )packetBuffer[43];
      return secsSince1900 - 2208988800UL + TIME_ZONE * 3600;
    }
  }
  // Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}
