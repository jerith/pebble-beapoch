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


Window *window;

Layer *background_layer;
SliderLayer *slider_wday_layer;
TextLayer *text_date_layer;
TextLayer *text_time_layer;
TextLayer *text_unix_layer;
TextLayer *text_beat_layer;
TextLayer *text_utco_layer;


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


char *int_to_str(long num) {
    // Maximum of then digits, because unix timestamp. Make sure to copy the
    // text out of the returned buffer before calling this again.
    static char text_num[] = "0123456789";

    for (int i = 9; i >= 0; i--) {
        text_num[i] = '0' + (num % 10);
        num /= 10;
        if (num == 0) {
            // Return from the middle of our string.
            return text_num + i;
        }
    }
    return text_num;
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
    strcpy(unix_text, int_to_str(unix_seconds));
    text_layer_set_text(text_unix_layer, unix_text);

    // Swatch .beats.

    beat_text[1] = '\0';
    strcat(beat_text, int_to_str(calc_swatch_beats(unix_seconds)));
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


static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
    if (request_timezone_tries_left) {
        request_timezone();
    }
    display_time(tick_time);
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

    time_t now = time(NULL);
    display_time(localtime(&now));

    request_timezone();
    tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
}


static void window_unload(Window *window) {
    tick_timer_service_unsubscribe();
    text_layer_destroy(text_utco_layer);
    text_layer_destroy(text_beat_layer);
    text_layer_destroy(text_unix_layer);
    text_layer_destroy(text_time_layer);
    text_layer_destroy(text_date_layer);
    slider_layer_destroy(slider_wday_layer);
    layer_destroy(background_layer);
}


void in_received_handler(DictionaryIterator *received, void *context) {
    Tuple *utc_offset_tuple = dict_find(received, BEAPOCH_KEY_UTC_OFFSET);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received timezone: %s", utc_offset_tuple->value->cstring);

    set_timezone_offset(utc_offset_tuple->value->cstring);
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


static void init(void) {
    set_timezone_offset(DEFAULT_UTC_OFFSET);

    // AppMessage setup.
    app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_register_outbox_failed(out_failed_handler);
    app_message_open(64, 64);

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
}


int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
