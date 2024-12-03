#include <Arduino.h>

// For Eink Reader
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/Picopixel.h> //Small font
#include <Fonts/FreeSansBoldOblique9pt7b.h> //Bigger font

// For Wifi
#include <WiFi.h>
#include <HTTPClient.h> // Query Stock Market via HTTP
#include "secrets.h"

#include "rest_queries.h"

// Eink Pinouts for ESP32 Wroom
#define EPD_BUSY 22 // to EPD BUSY //HACK: I2C SCL
#define EPD_CS 15   // to EPD CS
#define EPD_RST 4   // to EPD RST
#define EPD_DC 2    // to EPD DC
#define EPD_SCK 18  // to EPD CLK
#define EPD_MISO 31 // Master-In Slave-Out //HACK not used, just to startup SPI
#define EPD_MOSI 23 // to EPD DIN

// MH-ET Live 2.9" 128 X 296
// source: https://forum.arduino.cc/t/help-with-waveshare-epaper-display-with-adafruit-huzzah32-esp32-feather-board/574300/8
GxEPD2_BW<GxEPD2_290_T94, GxEPD2_290_T94::HEIGHT> Display(GxEPD2_290_T94(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

static const uint8_t EPD_MAX_HEIGHT = 128;

Datetime_t datetime_now;
Poylgon_params_t polygon_params;

/* Function Prototypes*/
void init_eink();
void clear_eink();

float mapf(float x, float in_min, float in_max, float out_min, float out_max);

bool connect_wifi(uint32_t timeout_ms);
void update_eink_stocks();

void display_ticker(String ticker, uint16_t x, uint16_t y);
void display_unit(Timespan_t timespan, uint16_t x, uint16_t y);

AggResult_t results[MAX_BAR_NUMBER];

void setup()
{
    init_eink();

    bool res = connect_wifi(30000);

    if (!res)
    {
        Serial.println("Error!");
        return;
    }

    delay(1000);

    // update_time(&datetime_now);
    datetime_now.year = 2024;
    datetime_now.month = 12;
    datetime_now.day = 3;

    update_polygon_param_date(
        &datetime_now,
        &polygon_params,
        Timespan_t::TS_DAY,
        30);

    Serial.println("Start: " + polygon_params.start_date);
    Serial.println("End:   " + polygon_params.end_date);

    delay(1000);

    // Stock Market doesnt work on weekends?
    String ticker = "AAPL"; //Apple

    query_stock_market(
        &polygon_params,
        ticker, 1,
        results, MAX_BAR_NUMBER);

    Serial.println("Query Done");

    clear_eink();
    Display.display();

    update_eink_stocks();
    
    display_ticker(ticker, 180, 50); //This font uses baseline
    display_unit(Timespan_t::TS_DAY, 180, 55);


}

void loop()
{
    delay(1000);
}

/* Function Definitions ------------------------------------------------------------------*/

void init_eink()
{
    /**
     * @brief Init eink SPI pins and configure display settings like uart, rotation, font
     * and text colour
     *
     */
    SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
    delay(100);

    Display.init(115200);   // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
    Display.setRotation(3); // landscape orientaion
    Display.setFullWindow();
    Display.setFont(); // display.setFont(&Picopixel);
    Display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
    Display.firstPage();
}

void clear_eink()
{
    Display.firstPage(); // Completely clear screen
    do
    {
        Display.fillScreen(GxEPD_WHITE);
        delay(10);
    } while (Display.nextPage());
    Display.fillScreen(GxEPD_WHITE);
    delay(100);
}

/**
 * @brief Uses WiFi library to connect to ssid and password in secrets.h
 *
 * @param timeout_ms
 * @return true
 * @return false timeout_ms reached without wifi connection
 */
bool connect_wifi(uint32_t timeout_ms)
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t time_start = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        Serial.print(".");

        if (millis() - time_start >= timeout_ms)
        {
            Serial.println("Wifi connection failed, timeout");
            return false;
        }
    }

    Serial.println("Wifi connection ok");
    return true;
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief Based on the results array, draw the stock charts
 * 
 */
void update_eink_stocks()
{
    //First look for the graph bounds
    float min_val = results[0].l_lowest_price;
    float max_val = results[0].h_highest_price;

    for(int i = 1; i < MAX_BAR_NUMBER; i++)
    {
        min_val = min(min_val, results[i].l_lowest_price);
        max_val = max(max_val, results[i].h_highest_price);
    }

    Serial.println(min_val);
    Serial.println(max_val);

    //Draw each stock on the display
    const int32_t bar_width = 11;
    const int32_t bar_gap = 1;    //Gap between bars

    int32_t bar_x_start = 0;
    int32_t line_x_start = 5; //middle of bar width //very strange...

    int32_t bar_y_start;
    int32_t line_y_start;

    float bar_y_ref;
    float line_y_ref;

    int32_t box_length;
    int32_t line_length;

    // go backwards since 0 is the most recent one.
    // want the leftmost to be the oldest data
    for(int i = MAX_BAR_NUMBER-1; i >= 0; i--)
    {
        //Find Box and Line Lengths
        box_length = abs(mapf(results[i].o_open_price, min_val, max_val, 0, EPD_MAX_HEIGHT) - 
                         mapf(results[i].c_close_price, min_val, max_val, 0, EPD_MAX_HEIGHT)
                         );

        line_length = abs(mapf(results[i].h_highest_price, min_val, max_val, 0, EPD_MAX_HEIGHT) - 
                          mapf(results[i].l_lowest_price, min_val, max_val, 0, EPD_MAX_HEIGHT)
                          );

        //Take reference from highest point
        if(results[i].c_close_price > results[i].o_open_price){
            bar_y_ref = results[i].c_close_price;
        }
        else bar_y_ref = results[i].o_open_price;

        if(results[i].h_highest_price > results[i].l_lowest_price){
            line_y_ref = results[i].h_highest_price;
        }
        else line_y_ref = results[i].l_lowest_price;

        // Map reference ot max min, remember top left corner is 0,0
        bar_y_start = (int) mapf(bar_y_ref, min_val, max_val, 0, EPD_MAX_HEIGHT);
        bar_y_start = EPD_MAX_HEIGHT - bar_y_start;

        line_y_start = (int) mapf(line_y_ref, min_val, max_val, 0, EPD_MAX_HEIGHT);
        line_y_start = EPD_MAX_HEIGHT - line_y_start;

        // Serial.println(String(i) + " Calculations");
        // Serial.println(String(bar_x_start) + " " + String(bar_y_start) + " " + String(bar_width) + " " + String(box_length));
        // Serial.println(String(line_x_start) + " " + String(line_y_start) + " " + String(line_length));

        //Drawing
        Display.drawRect(bar_x_start, bar_y_start, bar_width, box_length, GxEPD_BLACK);
        Display.drawFastVLine(line_x_start, line_y_start, line_length, GxEPD_BLACK);

        //Update of bar_x_start, line_x_start
        bar_x_start += bar_gap + bar_width;
        line_x_start += bar_gap + bar_width;

        Serial.println(i);

    }
    
    
    Display.setTextSize(1);

    Display.setCursor(bar_x_start,0);
    Display.print("Highest: $"); Display.print(max_val);

    Display.setCursor(bar_x_start,120);
    Display.print("Lowest : $"); Display.print(min_val);

    Display.display();

}

/**
 * @brief Displays the stock that is being printed
 * 
 * @param ticker 
 */
void display_ticker(String ticker, uint16_t x, uint16_t y)
{   
    Display.setFont(&FreeSansBoldOblique9pt7b);
    Display.setTextSize(2);
    Display.setCursor(x,y);
    Display.print(ticker);

    Display.display();
}

void display_unit(Timespan_t timespan, uint16_t x, uint16_t y)
{
    Display.setFont();
    Display.setTextSize(1);
    Display.setCursor(x,y);
    Display.print(get_timespan_str(timespan));

    Display.display();
}
