#include <WiFi.h>
#include <WiFiUDP.h>
#include <HTTPClient.h>
#include <SPI.h>

#include <DHT.h>
#include <TimeLib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "PrvSetting.h"

Adafruit_ILI9341 tft = Adafruit_ILI9341( 5, 17, TFT_RST );
WiFiUDP UDP_NTP;
WiFiUDP UDP_RCV;
DHT dht( 33, DHT11 );

/* 曜日変換用の文字列 */
const char week_day[7][8] = { "(SUN)", "(MON)", "(TUE)", "(WED)", "(THU)", "(FRI)", "(SAT)" };
/* 温湿度データ */
float humi = 0.0;
float temp = 0.0;
/* 現在時刻(日本時間)を取得 */
time_t now_data = 0;
/* UDP通信用 */
char udp_buff[24] = {};
/* テキスト色設定 */
uint16_t text_color = ILI9341_DARKCYAN;

uint8_t  wifi_init( void );
void     set_display( uint8_t h_data, uint8_t m_data, uint8_t s_data );
void     read_sensor( void );
void     udp_rcv( void );
uint16_t str_position( uint16_t str_length, uint16_t unit_length );
void     run_cgi( String url );
void     tft_wake( void );
void     tft_sleep( void );
time_t  getNtpTime( void );
void     sendNTPpacket( const char* address );

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

  Serial.begin( SERIAL_ESP32 );
  
  /* 温湿度センサーの開始 */
  dht.begin();

  /* wi-fi通信の開始 */
  flag_wifiinit_err = wifi_init();

  /* UDP通信の開始 */
  if( !UDP_NTP.begin( NTP_PORT ) || !UDP_RCV.begin( DATA_RCV_PORT ) )
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
    for(;;)
    { 
      delay( 3000 );
      ESP.restart();
      delay( 3000 ); /* 未到達 */
    }
  }
  else
  { 
    tft.fillScreen( ILI9341_BLACK ); /* 画面表示クリア */
    
    sprintf( udp_buff, "%s", UDP_DEFAULT );
    run_cgi( CGI_URL1 );
    
    /* 成功時 */
    setSyncProvider( getNtpTime ); /* 補正に使用する関数設定 */
    setSyncInterval( 3600 );       /* 時刻補正を行う周期設定(秒) 後で調整 */

    /* 平日の祝日か */
    flag_holidayjdg = get_daydata( ( uint8_t )month( now() ), day( now() ) );
    
    /* 温湿度データの取得(初回) */
    read_sensor();
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
      if( 31.0 < temp )
      {
        run_cgi( CGI_URL3 );
      }
      
      /* 画面のスリープ処理 */
      tft.fillScreen( ILI9341_BLACK );
      tft_sleep();
      ledcWrite( 0, 0 ); /* バックライト消灯 */
      
      /* deep-sleep */
      Serial.println( "deep-sleep start" );
      esp_sleep_enable_timer_wakeup( ( uint32_t )( 1800 * 1000 * 1000 ) );
      Serial.flush(); 
      esp_deep_sleep_start();
      delay( 1000 );
    }
    else
    {
      /* 画面表示 */
      set_display( h_data, m_data, s_data );
    }

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
    udp_rcv();
  }

  delay( 100 );
}

/* wi-fiの初期化関数 */
uint8_t wifi_init( void )
{
  /* 子機側に設定 */
  WiFi.mode( WIFI_STA );
  
  if( !WiFi.begin( AP_SSID, AP_PASS ) )
  {
    /* 失敗時 */
    return 1;
  }

  /* wi-fiの接続待ち */
  for( uint8_t loop = 0; loop < 10; loop++ )
  {
    if( WL_CONNECTED == WiFi.status() )
    {
      /* 成功時 */
      break; /* 接続できたらループ終了 */
    }
    delay( 500 );
  }

  if( WL_CONNECTED != WiFi.status() )
  {
    /* 失敗時 */
    return 1;
  }

  /* 成功時(未到達) */
  return 0;
}

/* OLEDに描画する関数 */
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

    /* 文字数取得 */
    String udp_str = udp_buff;
    String sensor_str = sensor_data;
    /* 画面上の文字の長さ */
    uint16_t udp_strlen = udp_str.length() * 12;
    uint16_t sensor_strlen = sensor_str.length() * 18;
    /* 中央寄せに使用するカーソル開始位置 */
    uint16_t udp_cursor = str_position( udp_strlen, 10 );
    uint16_t sensor_cursor = str_position( sensor_strlen, 18 );

    /* 文字数が異なる場合のリフレッシュ */
    if( ( udpstr_pre != udp_str.length() ) || ( ssrstr_pre != sensor_str.length() ) )
    {
      tft.fillRect( 0, 70, 320, 50, ILI9341_BLACK ); /* 温度情報の画面表示クリア */
      udpstr_pre = udp_str.length();                 /* UDP受信文字数の前回値更新 */
      ssrstr_pre = sensor_str.length();              /* センサー読み取り文字数の前回値更新 */
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

/* UDP受信処理関数 */
void udp_rcv( void )
{
  /* UDPのデータ長 */
  int udp_len = 0;
  
  if( 0 < UDP_RCV.parsePacket() )
  {
    udp_len = UDP_RCV.read( udp_buff, 24 );

    if( 0 < udp_len )
    {
      // Serial.println("Get Temperature Data");
      udp_buff[udp_len] = '\0';
    }
  }
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

/* TFT上の文字列を中央寄せに調整する */
uint16_t str_position( uint16_t str_length, uint16_t unit_length )
{
  uint16_t half_width = 0;
  uint16_t half_strlen = 0;
  
  half_width = ( SCREEN_WIDTH - unit_length ) / 2;
  half_strlen = str_length / 2;

  return ( half_width - half_strlen );
}

/* CGI実行 */
void run_cgi( String url )
{
  HTTPClient client;

  int http_get;

  /* タイムアウト時間の設定(15s) */
  client.setTimeout( 300 );
  
  client.begin( url );
  http_get = client.GET();

  if( 0 > http_get )
  {
    Serial.println( client.errorToString( http_get ) );
  }

  client.end();
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

/***

ここから下記コードを使用(一部変数の型などを変更しています)
ttps://github.com/PaulStoffregen/Time/blob/master/examples/TimeNTP/TimeNTP.ino

***/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (UDP_NTP.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(TIME_SERVER);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = UDP_NTP.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      // Serial.println("Receive NTP Response");
      UDP_NTP.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + TIME_ZONE * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(const char* address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  UDP_NTP.beginPacket(address, 123); //NTP requests are to port 123
  UDP_NTP.write(packetBuffer, NTP_PACKET_SIZE);
  UDP_NTP.endPacket();
}

/***

ここまで

***/
