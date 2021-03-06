#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "http.h"
#include "util.h"
#include "weather_layer.h"
#include "time_layer.h"
#include "link_monitor.h"
#include "config.h"

#define ANTONIO
#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }

PBL_APP_INFO(MY_UUID,
			"Antonio Weather", "Antonio Asaro", // Modification of "Futura Weather" by Niknam Moslehi which is a modification of "Roboto Weather" by Martin Rosinski
             1, 3, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);

#define TIME_FRAME      (GRect(0, 0, 144, 168-6))
#define DATE_FRAME      (GRect(1, 60, 144, 168-62))
#define UPDATED_FRAME   (GRect(78, 150, 144, 168))
#define MINMAX_FRAME    (GRect(82, 100, 144, 20))

// POST variables
#define WEATHER_KEY_LATITUDE 1
#define WEATHER_KEY_LONGITUDE 2
#define WEATHER_KEY_UNIT_SYSTEM 3
	
// Received variables
#define WEATHER_KEY_ICON 1
#define WEATHER_KEY_TEMP 2
#define WEATHER_KEY_MIN_TEMP 3
#define WEATHER_KEY_MAX_TEMP 4
	
#define WEATHER_HTTP_COOKIE 1949327671
#define TIME_HTTP_COOKIE 1131038282

Window window;          /* main window */
TextLayer date_layer;   /* layer for the date */
TimeLayer time_layer;   /* layer for the time */
TextLayer updated_layer;	/* layer for the updated time */
TextLayer minmax_layer;	

GFont font_date;        /* font for date */
GFont font_hour;        /* font for hour */
GFont font_minute;      /* font for minute */
GFont font_updated;     /* font for updated time */

static int initial_minute;
static PblTm updated_tm;

//Weather Stuff
static int our_latitude, our_longitude;
static bool located = false;
static bool initial_request = true;
static bool has_temperature = false;

WeatherLayer weather_layer;

void request_weather();

// Show the updated info
void set_updated() {
    static char updated_hour_text[] = "00";
    static char updated_minute_text[] = ":00";
	static char updated_info[] = "        "; 		// @00:00pm

	if (clock_is_24h_style()) {
		string_format_time(updated_hour_text, sizeof(updated_hour_text), "%H", &updated_tm);
	}
	else {
	    string_format_time(updated_hour_text, sizeof(updated_hour_text), "%I", &updated_tm); 
	}
	if (updated_hour_text[0] == '0') memmove(&updated_hour_text[0], &updated_hour_text[1], sizeof(updated_hour_text) - 1);
   	string_format_time(updated_minute_text, sizeof(updated_minute_text), ":%M", &updated_tm);
		
	strcpy(updated_info, "@");
	strcat(updated_info, updated_hour_text);
	strcat(updated_info, updated_minute_text);
	if (updated_tm.tm_hour < 12) strcat(updated_info, "am"); else strcat(updated_info, "pm");
	text_layer_set_text(&updated_layer, updated_info);
}

void failed(int32_t cookie, int http_status, void* context) {
	if(cookie == 0 || cookie == WEATHER_HTTP_COOKIE) {
#ifndef ANTONIO
		weather_layer_set_icon(&weather_layer, WEATHER_ICON_NO_WEATHER);
		text_layer_set_text(&weather_layer.temp_layer, "---°   ");
#endif
	}
	
	link_monitor_handle_failure(http_status);
	
	//Re-request the location and subsequently weather on next minute tick
	located = false;
}

void success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
	if(cookie != WEATHER_HTTP_COOKIE) return;
	Tuple* icon_tuple = dict_find(received, WEATHER_KEY_ICON);
	if(icon_tuple) {
		int icon = -1;
		char ipng[16];
		strcpy(ipng, icon_tuple->value->cstring);
		
		// map openweather to futura-weather icons
		// 01d.png	 01n.png	 sky is clear
		// 02d.png	 02n.png	 few clouds
		// 03d.png	 03n.png	 scattered clouds
		// 04d.png	 04n.png	 broken clouds
		// 09d.png	 09n.png	 shower rain
		// 10d.png	 10n.png	 Rain
		// 11d.png	 11n.png	 Thunderstorm
		// 13d.png	 13n.png	 snow
		// 50d.png	 50n.png	 mist
	    if (strcmp(ipng, "01d") == 0) icon = WEATHER_ICON_CLEAR_DAY;
	    if (strcmp(ipng, "01n") == 0) icon = WEATHER_ICON_CLEAR_NIGHT;
	    if (strcmp(ipng, "02d") == 0) icon = WEATHER_ICON_PARTLY_CLOUDY_DAY;
	    if (strcmp(ipng, "02n") == 0) icon = WEATHER_ICON_PARTLY_CLOUDY_NIGHT;
	    if (strcmp(ipng, "03d") == 0) icon = WEATHER_ICON_CLOUDY;
	    if (strcmp(ipng, "03n") == 0) icon = WEATHER_ICON_CLOUDY;
	    if (strcmp(ipng, "04d") == 0) icon = WEATHER_ICON_CLOUDY;
	    if (strcmp(ipng, "04n") == 0) icon = WEATHER_ICON_CLOUDY;
	    if (strcmp(ipng, "09d") == 0) icon = WEATHER_ICON_RAIN;
	    if (strcmp(ipng, "09n") == 0) icon = WEATHER_ICON_RAIN;
	    if (strcmp(ipng, "10d") == 0) icon = WEATHER_ICON_RAIN;
	    if (strcmp(ipng, "10n") == 0) icon = WEATHER_ICON_RAIN;
	    if (strcmp(ipng, "11d") == 0) icon = WEATHER_ICON_RAIN;
	    if (strcmp(ipng, "11n") == 0) icon = WEATHER_ICON_RAIN;
	    if (strcmp(ipng, "13d") == 0) icon = WEATHER_ICON_SNOW;
	    if (strcmp(ipng, "13n") == 0) icon = WEATHER_ICON_SNOW;
	    if (strcmp(ipng, "50d") == 0) icon = WEATHER_ICON_FOG;
	    if (strcmp(ipng, "50n") == 0) icon = WEATHER_ICON_FOG;	
		
		
		if(icon >= 0 && icon < 10) {
			weather_layer_set_icon(&weather_layer, icon);
		} else {
#ifndef ANTONIO
			weather_layer_set_icon(&weather_layer, WEATHER_ICON_NO_WEATHER);
#endif
		}
	}
	
	int min_temp = -1;
	int max_temp = -1;
	static char minmax[32];
	
	Tuple* min_temp_tuple = dict_find(received, WEATHER_KEY_MIN_TEMP);
	if (min_temp_tuple) { min_temp = min_temp_tuple->value->int16; }
	Tuple* max_temp_tuple = dict_find(received, WEATHER_KEY_MAX_TEMP);
	if (max_temp_tuple) { max_temp = max_temp_tuple->value->int16; }
	strcpy(minmax, itoa(max_temp)); strcat(minmax, "°/ ");
	strcat(minmax, itoa(min_temp)); strcat(minmax, "°");
	if (min_temp_tuple && max_temp_tuple) { text_layer_set_text(&minmax_layer, minmax); }

	Tuple* temperature_tuple = dict_find(received, WEATHER_KEY_TEMP);
	if (temperature_tuple) {
		weather_layer_set_temperature(&weather_layer, temperature_tuple->value->int16);
       	get_time(&updated_tm); set_updated(); 	// updated last successful wweather event
		has_temperature = true;
	}
	
	link_monitor_handle_success();
}

void location(float latitude, float longitude, float altitude, float accuracy, void* context) {
	// Fix the floats
	our_latitude = latitude * 10000;
	our_longitude = longitude * 10000;
	located = true;
	request_weather();
}

void reconnect(void* context) {
	located = false;
	request_weather();
}

void request_weather();

/* Called by the OS once per minute. Update the time and date.
*/
void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t)
{
    /* Need to be static because pointers to them are stored in the text
    * layers.
    */
    static char date_text[] = "XXX 00/00";
    static char hour_text[] = "00";
    static char minute_text[] = ":00";

    (void)ctx;  /* prevent "unused parameter" warning */

    if (t->units_changed & DAY_UNIT)
    {		
	    string_format_time(date_text,
                           sizeof(date_text),
                           "%a %m/%d",
                           t->tick_time);

		if (date_text[4] == '0') /* is day of month < 10? */
		{
		    /* This is a hack to get rid of the leading zero of the
			   day of month
            */
            memmove(&date_text[4], &date_text[5], sizeof(date_text) - 1);
		}
        text_layer_set_text(&date_layer, date_text);
    }

    if (clock_is_24h_style())
    {
        string_format_time(hour_text, sizeof(hour_text), "%H", t->tick_time);
		if (hour_text[0] == '0')
        {
            /* This is a hack to get rid of the leading zero of the hour.
            */
            memmove(&hour_text[0], &hour_text[1], sizeof(hour_text) - 1);
        }
    }
    else
    {
        string_format_time(hour_text, sizeof(hour_text), "%I", t->tick_time);
        if (hour_text[0] == '0')
        {
            /* This is a hack to get rid of the leading zero of the hour.
            */
            memmove(&hour_text[0], &hour_text[1], sizeof(hour_text) - 1);
        }
    }
	


    string_format_time(minute_text, sizeof(minute_text), ":%M", t->tick_time);
    time_layer_set_text(&time_layer, hour_text, minute_text);
	
	if(initial_request || !has_temperature || (t->tick_time->tm_min % 30) == initial_minute)
	{
		// Every 30 minutes, request updated weather
		http_location_request();
		initial_request = false;
	}
	else
	{
		// Ping the phone every minute
		link_monitor_ping();
	}
}

/* Initialize the application.
*/
void handle_init(AppContextRef ctx)
{
    PblTm tm;
    PebbleTickEvent t;
    ResHandle res_d;
    ResHandle res_h;
//    ResHandle res_m;
	ResHandle res_u;

    window_init(&window, "Antonio");
    window_stack_push(&window, true /* Animated */);
    window_set_background_color(&window, GColorBlack);

    resource_init_current_app(&APP_RESOURCES);

    res_d = resource_get_handle(RESOURCE_ID_FUTURA_18);
    res_h = resource_get_handle(RESOURCE_ID_FUTURA_CONDENSED_53);
	res_u = resource_get_handle(RESOURCE_ID_FUTURA_12);

    font_date = fonts_load_custom_font(res_d);
    font_hour = fonts_load_custom_font(res_h);
    font_minute = fonts_load_custom_font(res_h);
    font_updated = fonts_load_custom_font(res_u);

    time_layer_init(&time_layer, window.layer.frame);
    time_layer_set_text_color(&time_layer, GColorWhite);
    time_layer_set_background_color(&time_layer, GColorClear);
    time_layer_set_fonts(&time_layer, font_hour, font_minute);
    layer_set_frame(&time_layer.layer, TIME_FRAME);
    layer_add_child(&window.layer, &time_layer.layer);

    text_layer_init(&date_layer, window.layer.frame);
    text_layer_set_text_color(&date_layer, GColorWhite);
    text_layer_set_background_color(&date_layer, GColorClear);
    text_layer_set_font(&date_layer, font_date);
    text_layer_set_text_alignment(&date_layer, GTextAlignmentCenter);
    layer_set_frame(&date_layer.layer, DATE_FRAME);
    layer_add_child(&window.layer, &date_layer.layer);

	// Add weather layer
	weather_layer_init(&weather_layer, GPoint(0, 90));
	layer_add_child(&window.layer, &weather_layer.layer);
	
	// Add updated layer
    text_layer_init(&updated_layer, window.layer.frame);
    text_layer_set_text_color(&updated_layer, GColorBlack);
    text_layer_set_background_color(&updated_layer, GColorClear);
    text_layer_set_font(&updated_layer, font_updated);
    text_layer_set_text_alignment(&updated_layer, GTextAlignmentLeft);
    layer_set_frame(&updated_layer.layer, UPDATED_FRAME);
    layer_add_child(&window.layer, &updated_layer.layer);

	// Add hi/low layer
    text_layer_init(&minmax_layer, window.layer.frame);
    text_layer_set_text_color(&minmax_layer, GColorBlack);
    text_layer_set_background_color(&minmax_layer, GColorClear);
    text_layer_set_font(&minmax_layer, font_updated);
    text_layer_set_text_alignment(&minmax_layer, GTextAlignmentLeft);
    layer_set_frame(&minmax_layer.layer, MINMAX_FRAME);
    layer_add_child(&window.layer, &minmax_layer.layer);

	
	
	http_register_callbacks((HTTPCallbacks){.failure=failed,.success=success,.reconnect=reconnect,.location=location}, (void*)ctx);
	
	// Refresh time
	get_time(&tm);
    t.tick_time = &tm;
    t.units_changed = SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT;
	
	initial_minute = (tm.tm_min % 30);
//	text_layer_set_text(&updated_layer, "@ --:--");
	
	handle_minute_tick(ctx, &t);
}

/* Shut down the application
*/
void handle_deinit(AppContextRef ctx)
{
    fonts_unload_custom_font(font_date);
    fonts_unload_custom_font(font_hour);
    fonts_unload_custom_font(font_minute);
	fonts_unload_custom_font(font_updated);
	
	weather_layer_deinit(&weather_layer);
}


/********************* Main Program *******************/

void pbl_main(void *params)
{
    PebbleAppHandlers handlers =
    {
        .init_handler = &handle_init,
        .deinit_handler = &handle_deinit,
        .tick_info =
        {
            .tick_handler = &handle_minute_tick,
            .tick_units = MINUTE_UNIT
        },
		.messaging_info = {
			.buffer_sizes = {
				.inbound = 124,
				.outbound = 256,
			}
		}
    };

    app_event_loop(params, &handlers);
}

void request_weather() {
    DictionaryIterator *body;
    static char url[256];
    static char lon[64];
    static char lat[64];
    static char unt[64];

	if(!located) {
		http_location_request();
		return;
	}

	strcpy(url, "http://antonioasaro.site50.net/weather_min_max.php");
	strcpy(lat, "?lat="); strcat(lat, itoa(our_latitude)); 
	strcpy(lon, "&lon="); strcat(lon, itoa(our_longitude));
    strcpy(unt, "&unt="); strcat(unt, "metric");                    // or "imperial"
    strcat(url, lat); strcat(url, lon); strcat(url, unt);

 	if (http_out_get(url, false, WEATHER_HTTP_COOKIE, &body) != HTTP_OK ||
        http_out_send() != HTTP_OK) {
		return;
    }

}