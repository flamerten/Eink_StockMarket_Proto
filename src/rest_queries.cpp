/**
 * @file rest_queries.cpp
 * @author Samuel Yow
 * @brief 
 * @version 0.1
 * @date 2024-12-03
 * 
 * For querying stock prices and the current datetime using REST
 * 
 */

#include "rest_queries.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// Private Methods ------------------------------------------------

static uint32_t convert_seconds(Timespan_t timespan, uint32_t number)
{   
    uint32_t result;

    switch(timespan)
    {
        case(TS_SECOND):
            result = number;
            break;
        case(TS_MINUTE):
            result = number * 60;
            break;
        case(TS_HOUR):
            result = number * 60 * 60;
            break;
        case (TS_DAY):
            result = number * 60 * 60 * 24;
            break;
        case(TS_WEEK):
            result = number * 60 * 60 * 24 * 7;
            break;
        case (TS_MONTH):
            result = number * 60 * 60 * 24 * 7 * 4; //4 weeks a month
            break;
        case (TS_QUARTER):
            result = number * 60 * 60 * 24 * 7 * 4 * 3; // 3 months a quarter
            break;
        case (TS_YEAR):
            result = number * 60 * 60 * 24 * 365; // 365 days
            break;
        default:
            Serial.println("Error invalid timespan");
            break;
    }

    return result;
}

/**
 * @brief Get the stock market data object, results are in desc, newest first
 * 
 * @param polygon_params 
 * @param ticker Case sensitive ticker symbol, AAPL is Apple inc
 * @param multiplier timespan multiplier
 * @return String 
 */
static String get_server_query(
    Poylgon_params_t* polygon_params,
    String ticker,
    uint8_t multiplier)
{
    polygon_params->ticker = ticker;
    polygon_params->multiplier = multiplier;

    char query[1000];

    ///v2/aggs/ticker/{stocksTicker}/range/{multiplier}/{timespan}/{from}/{to}
    snprintf(query, sizeof(query),
        "%s/aggs/ticker/%s/range/%i/%s/%s/%s?%s&apiKey=%s",
        "https://api.polygon.io/v2",
        polygon_params->ticker,
        polygon_params->multiplier,
        polygon_params->timespan,
        polygon_params->start_date,
        polygon_params->end_date,
        "adjusted=true&sort=desc",
        POLYGON_API_KEY
    );

    return String(query);

}


static String http_GET_request(const char* serverName) {
  HTTPClient http;

  http.begin(serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

// Public Methods --------------------------------------------------

/**
 * @brief Update the datetime struct with the current datetime using the NTP Client
 * 
 * @param datetime 
 */
void update_time(Datetime_t* datetime)
{       
    int8_t max_tc_updates = 5;

    while(!timeClient.update()){
        timeClient.forceUpdate();
        max_tc_updates--;
        if(max_tc_updates <= 0){
            Serial.println("Time Client failed to update"); //Simply use RTC
            datetime->error = true;
            return;
        }
    }

    //Format: 2018-04-30T16:00:13Z 
    //String formattedDate = "2018-04-30T16:00:13Z";
    String formattedDate = timeClient.getFormattedDate();

    // Extract date
    int idx_t = formattedDate.indexOf("T");
    String date_ymd = formattedDate.substring(0, idx_t);

    //Serial.println("Orig:" + date_ymd);

    int idx_colon_fir = date_ymd.indexOf("-");
    int idx_colon_sec = date_ymd.indexOf("-", idx_colon_fir + 1);

    String year  = date_ymd.substring(0,idx_colon_fir);
    String month = date_ymd.substring(idx_colon_fir + 1, idx_colon_sec);
    String day   = date_ymd.substring(idx_colon_sec + 1);

    //timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
    //Serial.println(String(year) + "-"  + String(month) + "-" + String(day));

    datetime->year = year.toInt();
    datetime->month = month.toInt();
    datetime->day = day.toInt();
  
}

String get_timespan_str(Timespan_t timespan)
{   
    if      (timespan == TS_SECOND)  return "second";
    else if (timespan == TS_MINUTE)  return "minute";
    else if (timespan == TS_HOUR)    return "hour";
    else if (timespan == TS_DAY)     return "day";
    else if (timespan == TS_WEEK)    return "week";
    else if (timespan ==TS_MONTH)    return "month";
    else if (timespan == TS_QUARTER) return "quarter";
    else if (timespan == TS_YEAR)    return "year";
    else{
        Serial.println("Error, invalid timespan");
        return "";
    }
}

/**
 * @brief Update Polygon params with the timespan, startdate and enddate
 * where the end date is time_diff * timespan away from the end date, which is now.
 * 
 * @param datetime 
 * @param polygon_params 
 * @param timespan 
 * @param time_diff 
 */
void update_polygon_param_date(
    Datetime_t* datetime, 
    Poylgon_params_t* polygon_params,
    Timespan_t timespan, 
    uint8_t time_diff)
{

    // Update polygon params with datenow and timespan
    polygon_params->timespan = get_timespan_str(timespan);

    String year_now  = String(datetime->year);
    String month_now = String(datetime->month);
    String day_now   = String(datetime->day);

    if(month_now.length() == 1) month_now = "0" + month_now;
    if(day_now.length() == 1) day_now = "0" + day_now;

    polygon_params->end_date = year_now + "-" + month_now 
        + "-" + day_now;
    
    // Convert to time_t for time calc
    struct tm tm = {0};
    tm.tm_year = datetime->year - 1900; // Years since 1900
    tm.tm_mon  = datetime->month - 1;     // Months are 0-11
    tm.tm_mday = datetime->day;

    time_t t = mktime(&tm);
    t -= convert_seconds(timespan, time_diff);

    struct tm *adjusted_date = localtime(&t);

    //Update the start_date in polygon params
    String year_prev = String(adjusted_date->tm_year + 1900);
    String month_prev = String(adjusted_date->tm_mon + 1);
    String day_prev = String(adjusted_date->tm_mday);

    if(month_prev.length() == 1) month_prev = "0" + month_prev;
    if(day_prev.length() == 1) day_prev = "0" + day_prev;

    polygon_params->start_date = year_prev + "-" + month_prev 
        + "-" + day_prev;
}

/**
 * @brief 
 * 
 * @param polygon_params 
 * @param ticker 
 * @param multiplier 
 * @param agg_result_arr update this, where 0 is the most recent result
 * @param max_bar_no 
 * @return uint32_t number of aggs filled
 */
uint32_t query_stock_market(
    Poylgon_params_t* polygon_params,
    String ticker,
    uint8_t multiplier,
    AggResult_t* agg_result_arr,
    uint32_t max_bar_no)
{   
    uint32_t result_size = 0;
    JsonDocument json_doc;

    uint8_t attempts = 0;

    do
    {   
        attempts++;
    
        String server_query = get_server_query(polygon_params, ticker, multiplier);
        String payload = http_GET_request(server_query.c_str());

        DeserializationError error = deserializeJson(json_doc, payload);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            continue;
        }

        result_size = json_doc["results"].size();
        
        if (result_size == 0)
        {
            delay(1000);
            Serial.println("Trying Again...");
        }

        if(attempts >= 10)
        {
            return 0; //0 results, failed
        }

    } while (result_size == 0);

    if(max_bar_no < result_size)
    {
        Serial.println("Note: Queried data has a length > maxlen" + String(result_size));
    }

    Serial.println(json_doc["ticker"].as<const char*>());
    Serial.println(json_doc["results"].size());
    
    for(int i = 0; i < MAX_BAR_NUMBER; i++)
    {

        Serial.println(String(i) + "/" + String(result_size) + " " + json_doc["results"][i]["h"].as<float>());

        agg_result_arr[i].c_close_price          = json_doc["results"][i]["c"].as<float>();
        agg_result_arr[i].h_highest_price        = json_doc["results"][i]["h"].as<float>();
        agg_result_arr[i].l_lowest_price         = json_doc["results"][i]["l"].as<float>();
        agg_result_arr[i].o_open_price           = json_doc["results"][i]["o"].as<float>();
        agg_result_arr[i].v_trading_volume       = json_doc["results"][i]["v"].as<float>();
        agg_result_arr[i].vm_volume_weighted_avg = json_doc["results"][i]["vm"].as<float>();

        agg_result_arr[i].n_transactions  = json_doc["results"][i]["n"].as<uint32_t>();
        agg_result_arr[i].t_ts_agg_window = json_doc["results"][i]["t"].as<uint32_t>();
    }

    return result_size;
}