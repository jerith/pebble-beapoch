#include <pebble.h>

#include "slider.h"


// TODO: Fix this if and when we get access to timezone data.
#define DEFAULT_UTC_OFFSET "+0000"

static char utc_offset_str[] = "+0000";
static int utc_offset_seconds = 0;
static int request_timezone_tries_left = 10;  // Try this many times before giving up.
#define INT_FROM_DIGIT(X) ((int)X - '0')


enum {
  BEAPOCH_KEY_UTC_OFFSET = 0x0,
};


static Window *window;

static Layer *background_layer;
static SliderLayer *slider_wday_layer;
static TextLayer *text_date_layer;
static TextLayer *text_time_layer;
static TextLayer *text_unix_layer;
static TextLayer *text_beat_layer;
static TextLayer *text_utco_layer;

static BitmapLayer *image_bat_layer;
static BitmapLayer *image_btc_layer;

// TODO: Battery state indicators that match the built-in ones better.
static GBitmap *img_bat_charging;
static GBitmap *img_bat_full;
static GBitmap *img_bat_low;
static GBitmap *img_bt_disconnected;


#define BACKGROUND_COLOR GColorBlack
#define FOREGROUND_COLOR GColorWhite

/* #define LAYOUT_DEBUG */


#define TEXT_FG_COLOR FOREGROUND_COLOR
#define TEXT_BG_COLOR GColorClear
#define BORDER_COLOR FOREGROUND_COLOR
#define WINDOW_COLOR BACKGROUND_COLOR

#ifdef LAYOUT_DEBUG
#undef TEXT_FG_COLOR
#undef TEXT_BG_COLOR
#undef BORDER_COLOR
#undef WINDOW_COLOR
#define TEXT_FG_COLOR GColorWhite
#define TEXT_BG_COLOR GColorBlack
#define BORDER_COLOR GColorBlack
#define WINDOW_COLOR GColorWhite
#endif


// Some layout constants so we can tweak things more easily.

#define ISO_TOP 40
#define ISO_HPADDING 4
#define ISO_WDAY_HEIGHT 10
#define ISO_DATE_HEIGHT 26
#define ISO_TIME_HEIGHT 36
#define ISO_BORDER_PADDING 12

#define ISO_WIDTH (144 - 2 * ISO_HPADDING)
#define ISO_HEIGHT (ISO_WDAY_HEIGHT + ISO_DATE_HEIGHT + ISO_TIME_HEIGHT + 2 * (ISO_HPADDING + 2))
#define ISO_TEXT_WIDTH (ISO_WIDTH - 2 * (ISO_HPADDING + 2))
#define ISO_RECT GRect(ISO_HPADDING, ISO_TOP, ISO_WIDTH, ISO_HEIGHT)
#define ISO_WDAY_TOP (ISO_TOP + ISO_HPADDING + 2)
#define ISO_DATE_TOP (ISO_WDAY_TOP + ISO_WDAY_HEIGHT)
#define ISO_TIME_TOP (ISO_DATE_TOP + ISO_DATE_HEIGHT)
#define ISO_WDAY_RECT GRect(ISO_HPADDING + ISO_HPADDING + 2, ISO_WDAY_TOP, ISO_TEXT_WIDTH, ISO_WDAY_HEIGHT)
#define ISO_DATE_RECT GRect(ISO_HPADDING + ISO_HPADDING + 2, ISO_DATE_TOP, ISO_TEXT_WIDTH, ISO_DATE_HEIGHT)
#define ISO_TIME_RECT GRect(ISO_HPADDING + ISO_HPADDING + 2, ISO_TIME_TOP, ISO_TEXT_WIDTH, ISO_TIME_HEIGHT)

#define UNIX_TOP 0
#define UNIX_HPADDING 0
#define UNIX_HEIGHT 30
#define UNIX_WIDTH (144 - 2 * UNIX_HPADDING)
#define UNIX_RECT GRect(UNIX_HPADDING, UNIX_TOP, UNIX_WIDTH, UNIX_HEIGHT)

#define BEAT_TOP 136
#define BEAT_LEFT 70
#define BEAT_HEIGHT 32
#define BEAT_WIDTH (144 - BEAT_LEFT)
#define BEAT_RECT GRect(BEAT_LEFT, BEAT_TOP, BEAT_WIDTH, BEAT_HEIGHT)

#define UTCO_TOP 138
#define UTCO_LEFT 4
#define UTCO_HEIGHT 28
#define UTCO_WIDTH (BEAT_LEFT - 2 * UTCO_LEFT)
#define UTCO_RECT GRect(UTCO_LEFT, UTCO_TOP, UTCO_WIDTH, UTCO_HEIGHT)

#define IBAT_TOP 128
#define IBAT_LEFT (72 - 4 - 17)
#define IBAT_HEIGHT 10
#define IBAT_WIDTH 17
#define IBAT_RECT GRect(IBAT_LEFT, IBAT_TOP, IBAT_WIDTH, IBAT_HEIGHT)

#define IBTC_TOP 128
#define IBTC_LEFT (72 + 4)
#define IBTC_HEIGHT 10
#define IBTC_WIDTH 16
#define IBTC_RECT GRect(IBTC_LEFT, IBTC_TOP, IBTC_WIDTH, IBTC_HEIGHT)


#define RTOP(rect) (rect.origin.y)
#define RBOTTOM(rect) (rect.origin.y + rect.size.h)
#define RLEFT(rect) (rect.origin.x)
#define RRIGHT(rect) (rect.origin.x + rect.size.w)


void set_timezone_offset(char tz_offset[]) {
    int hour = 0;
    int min = 0;

    strncpy(utc_offset_str, tz_offset, sizeof(utc_offset_str));

    hour = (INT_FROM_DIGIT(tz_offset[1]) * 10) + INT_FROM_DIGIT(tz_offset[2]);
    min = (INT_FROM_DIGIT(tz_offset[3]) * 10) + INT_FROM_DIGIT(tz_offset[4]);

    utc_offset_seconds = hour*60*60 + min*60;
    if(tz_offset[0] != '+') {
        utc_offset_seconds = -1 * utc_offset_seconds;
    }
}


void draw_border_box(GContext *gctx, GRect rect, int corner_radius) {
    GRect inner_rect;
    inner_rect = GRect(rect.origin.x + 1, rect.origin.y + 1, rect.size.w - 2, rect.size.h - 2);
    // Double thick border
    graphics_draw_round_rect(gctx, rect, corner_radius);
    graphics_draw_round_rect(gctx, inner_rect, corner_radius);
    // Draw again with different corner_radius to fill in missing pixels
    graphics_draw_round_rect(gctx, rect, corner_radius + 1);
    graphics_draw_round_rect(gctx, inner_rect, corner_radius - 1);
}


void update_background_callback(Layer *layer, GContext *gctx) {
    (void) layer;

    graphics_context_set_stroke_color(gctx, BORDER_COLOR);
    graphics_context_set_fill_color(gctx, BORDER_COLOR);
    draw_border_box(gctx, ISO_RECT, ISO_BORDER_PADDING);
}


time_t calc_unix_seconds(struct tm *tick_time) {
    // This uses a naive algorithm for calculating leap years and will
    // therefore fail in 2100.
    int years_since_epoch;
    int days_since_epoch;

    years_since_epoch = tick_time->tm_year - 70;
    days_since_epoch = (years_since_epoch * 365 +
                        (years_since_epoch + 1) / 4 +
                        tick_time->tm_yday);
    return (days_since_epoch * 86400 +
            tick_time->tm_hour * 3600 +
            tick_time->tm_min * 60 +
            tick_time->tm_sec);
}


time_t calc_swatch_beats(time_t unix_seconds) {
    return (((unix_seconds + 3600) % 86400) * 1000) / 86400;
}


void display_time(struct tm *tick_time) {
    // Static, because we pass them to the system.
    static char date_text[] = "9999-99-99";
    static char time_text[] = "99:99:99";
    static char unix_text[] = "0123456789";
    static char beat_text[] = "@1000";

    time_t unix_seconds;

    // Weekday.

    // ISO says the day starts on Monday, Pebble says it starts on Sunday.
    slider_layer_set_position(slider_wday_layer, (uint8_t)(tick_time->tm_wday + 6) % 7);

    // Date.

    strftime(date_text, sizeof(date_text), "%Y-%m-%d", tick_time);
    text_layer_set_text(text_date_layer, date_text);

    // Time.

    strftime(time_text, sizeof(time_text), "%H:%M:%S", tick_time);
    if (time_text[0] == ' ') {
        time_text[0] = '0';
    }
    text_layer_set_text(text_time_layer, time_text);

    // Unix timestamp.

    unix_seconds = calc_unix_seconds(tick_time) - utc_offset_seconds;
    snprintf(unix_text, sizeof(unix_text), "%ld", unix_seconds);
    text_layer_set_text(text_unix_layer, unix_text);

    // Swatch .beats.

    snprintf(beat_text, sizeof(unix_text), "@%ld", calc_swatch_beats(unix_seconds));
    text_layer_set_text(text_beat_layer, beat_text);

    // Timezone offset.

    text_layer_set_text(text_utco_layer, utc_offset_str);
}


TextLayer *init_text_layer(GRect rect, GTextAlignment align, uint32_t font_res_id) {
    TextLayer *layer = text_layer_create(rect);
    text_layer_set_text_color(layer, TEXT_FG_COLOR);
    text_layer_set_background_color(layer, TEXT_BG_COLOR);
    text_layer_set_text_alignment(layer, align);
    text_layer_set_font(layer, fonts_load_custom_font(resource_get_handle(font_res_id)));
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(layer));
    return layer;
}


static void request_timezone(void) {
    Tuplet utc_offset_tuple = TupletInteger(BEAPOCH_KEY_UTC_OFFSET, 1);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Requesting timezone, tries left: %d", request_timezone_tries_left);
    if (request_timezone_tries_left > 0) {
        request_timezone_tries_left -= 1;
    }

    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    if (iter == NULL) {
        return;
    }

    dict_write_tuplet(iter, &utc_offset_tuple);
    dict_write_end(iter);

    app_message_outbox_send();
}


void in_received_handler(DictionaryIterator *received, void *context) {
    Tuple *utc_offset_tuple = dict_find(received, BEAPOCH_KEY_UTC_OFFSET);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received timezone: %s", utc_offset_tuple->value->cstring);

    set_timezone_offset(utc_offset_tuple->value->cstring);
    persist_write_string(BEAPOCH_KEY_UTC_OFFSET, utc_offset_tuple->value->cstring);

    if (request_timezone_tries_left > 0) {
        request_timezone_tries_left = 0;
    }
}


void in_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Dropped! (%u)", reason);
}


void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Send Failed! (%u)", reason);
}


static void get_stored_timezone(void) {
    char utc_offset_str[5];

    if (!persist_exists(BEAPOCH_KEY_UTC_OFFSET)) {
        // We don't have it persisted, so give up.
        return;
    }
    persist_read_string(BEAPOCH_KEY_UTC_OFFSET, utc_offset_str, sizeof(utc_offset_str));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Looked up UTC offset from local storage: %s", utc_offset_str);
    set_timezone_offset(utc_offset_str);

    // We don't need to poll the phone for this, because we already have it.
    if (request_timezone_tries_left > 0) {
        request_timezone_tries_left = 0;
    }
}


static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
    if (request_timezone_tries_left) {
        // If we don't already have a UTC offset, ask the phone for one.
        request_timezone();
    }
    display_time(tick_time);
}


static void handle_battery_state(BatteryChargeState charge) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Battery charge state: charging=%d plugged=%d level=%d%%", charge.is_charging, charge.is_plugged, charge.charge_percent);
    if (charge.is_charging) {
        bitmap_layer_set_bitmap(image_bat_layer, img_bat_charging);
        layer_set_hidden(bitmap_layer_get_layer(image_bat_layer), false);
    } else if (charge.is_plugged) {
        bitmap_layer_set_bitmap(image_bat_layer, img_bat_full);
        layer_set_hidden(bitmap_layer_get_layer(image_bat_layer), false);
    } else if (charge.charge_percent <= 10) {
        // This is a guess. It probably won't match the built-in indicator.
        bitmap_layer_set_bitmap(image_bat_layer, img_bat_low);
        layer_set_hidden(bitmap_layer_get_layer(image_bat_layer), false);
    } else {
        // We're unplugged and not low, so no indicator.
        layer_set_hidden(bitmap_layer_get_layer(image_bat_layer), true);
    }
}

static void handle_bluetooth_state(bool connected) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Bluetooth connection state: %d", connected);
    if (connected) {
        // Connected, so nothing to show.
        layer_set_hidden(bitmap_layer_get_layer(image_btc_layer), true);
    } else {
        // Disconnected, show icon.
        layer_set_hidden(bitmap_layer_get_layer(image_btc_layer), false);
    }
}


static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);

    window_set_background_color(window, WINDOW_COLOR);
    background_layer = layer_create(layer_get_bounds(window_layer));
    layer_set_update_proc(background_layer, &update_background_callback);
    layer_add_child(window_layer, background_layer);

    slider_wday_layer = slider_layer_create(ISO_WDAY_RECT, 7);
    slider_layer_set_line_color(slider_wday_layer, GColorClear);
    slider_layer_set_tick_color(slider_wday_layer, TEXT_FG_COLOR);
    slider_layer_set_indicator_color(slider_wday_layer, TEXT_FG_COLOR);
    slider_layer_set_tick_height(slider_wday_layer, 0);
    slider_layer_set_indicator_radius(slider_wday_layer, 2);
    layer_add_child(window_layer, slider_wday_layer);

    text_date_layer = init_text_layer(ISO_DATE_RECT, GTextAlignmentCenter, RESOURCE_ID_FONT_ISO_DATE_23);
    text_time_layer = init_text_layer(ISO_TIME_RECT, GTextAlignmentCenter, RESOURCE_ID_FONT_ISO_TIME_31);
    text_unix_layer = init_text_layer(UNIX_RECT, GTextAlignmentCenter, RESOURCE_ID_FONT_UNIX_TIME_23);
    text_beat_layer = init_text_layer(BEAT_RECT, GTextAlignmentLeft, RESOURCE_ID_FONT_SWATCH_BEATS_24);
    text_utco_layer = init_text_layer(UTCO_RECT, GTextAlignmentLeft, RESOURCE_ID_FONT_TZ_OFFSET_20);

    image_bat_layer = bitmap_layer_create(IBAT_RECT);
    bitmap_layer_set_alignment(image_bat_layer, GAlignCenter);
    layer_add_child(window_layer, bitmap_layer_get_layer(image_bat_layer));

    image_btc_layer = bitmap_layer_create(IBTC_RECT);
    bitmap_layer_set_bitmap(image_btc_layer, img_bt_disconnected);
    bitmap_layer_set_alignment(image_btc_layer, GAlignCenter);
    layer_add_child(window_layer, bitmap_layer_get_layer(image_btc_layer));

    get_stored_timezone();
    time_t now = time(NULL);
    display_time(localtime(&now));
    tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);

    handle_battery_state(battery_state_service_peek());
    battery_state_service_subscribe(handle_battery_state);

    handle_bluetooth_state(bluetooth_connection_service_peek());
    bluetooth_connection_service_subscribe(handle_bluetooth_state);
}


static void window_unload(Window *window) {
    bluetooth_connection_service_unsubscribe();
    battery_state_service_unsubscribe();
    tick_timer_service_unsubscribe();
    text_layer_destroy(text_utco_layer);
    text_layer_destroy(text_beat_layer);
    text_layer_destroy(text_unix_layer);
    text_layer_destroy(text_time_layer);
    text_layer_destroy(text_date_layer);
    slider_layer_destroy(slider_wday_layer);
    layer_destroy(background_layer);
}


static void init(void) {
    set_timezone_offset(DEFAULT_UTC_OFFSET);

    // AppMessage setup.
    app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_register_outbox_failed(out_failed_handler);
    app_message_open(64, 64);

    // Load bitmaps.
    img_bat_charging = gbitmap_create_with_resource(RESOURCE_ID_IMG_BAT_CHARGING);
    img_bat_full = gbitmap_create_with_resource(RESOURCE_ID_IMG_BAT_FULL);
    img_bat_low = gbitmap_create_with_resource(RESOURCE_ID_IMG_BAT_LOW);
    img_bt_disconnected = gbitmap_create_with_resource(RESOURCE_ID_IMG_BT_DISCONNECTED);

    // Window setup.
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
    window_stack_push(window, true /* Animated */);
}


static void deinit(void) {
    window_destroy(window);

    // Clean up bitmaps.
    gbitmap_destroy(img_bat_charging);
    gbitmap_destroy(img_bat_full);
    gbitmap_destroy(img_bat_low);
    gbitmap_destroy(img_bt_disconnected);
}


int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
