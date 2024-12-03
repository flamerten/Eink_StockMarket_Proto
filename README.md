# Eink Stock Market Prototype

For now, only queries APPLE stock on initialization for the last 14 days via [aggregate bars](https://polygon.io/docs/stocks/get_v2_aggs_ticker__stocksticker__range__multiplier___timespan___from___to) over each day. The current date is found using NTPClient and the polygon api.

To recreate this, create a `src/secrets.h` file with the following paramters.

```c
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "wifi_ssid"
#define WIFI_PASSWORD "wifipassword"
#define POLYGON_API_KEY "polygonapikey"

#endif //SECRETS_H
```

Note that the free tier of polygon only gives access to at most 5 API calls per minute, and the timeframe given is only end-of-day data.

To add the NTPClient Libary
```bash
cd lib
git clone https://github.com/taranais/NTPClient.git
```