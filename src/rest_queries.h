#ifndef REST_QUERIES_H
#define REST_QUERIES_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <time.h>
#include <HTTPClient.h>

#include "NTPClient.h" //From https://github.com/taranais/NTPClient to get formatted date

#include "secrets.h"

//Timer Server and Records for Singapore
#define NTP_OFFSET  28800 // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "sg.pool.ntp.org"

#define MAX_BAR_NUMBER 14

typedef struct{
    uint8_t day;
    uint8_t month;
    uint32_t year;
    bool error;
} Datetime_t;

typedef struct{
    String ticker;
    uint8_t multiplier;
    String timespan;
    String start_date;
    String end_date;
} Poylgon_params_t;

typedef enum TimeSpan{
    TS_SECOND = 1,
    TS_MINUTE = 2,
    TS_HOUR = 3,
    TS_DAY = 4,
    TS_WEEK = 5,
    TS_MONTH = 6,
    TS_QUARTER = 7,
    TS_YEAR = 8,
} Timespan_t;

typedef struct AggResult{
    float    c_close_price;
    float    h_highest_price;
    float    l_lowest_price;
    float    o_open_price;
    float    v_trading_volume;
    float    vm_volume_weighted_avg;

    uint32_t n_transactions;
    uint32_t t_ts_agg_window; //unix timestamp
} AggResult_t;

bool update_time(Datetime_t* datetime);

String get_timespan_str(Timespan_t timespan);

void update_polygon_param_date(
    Datetime_t* datetime, 
    Poylgon_params_t* polygon_params,
    Timespan_t timespan, 
    uint8_t time_diff);

uint32_t query_stock_market(
    Poylgon_params_t* polygon_params,
    String ticker,
    uint8_t multiplier,
    AggResult_t* results,
    uint32_t max_bar_no);

#endif // REST_QUERIES_H