#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "slider.h"

#define MY_UUID { 0xD8, 0xD3, 0x12, 0xA8, 0xDC, 0xA7, 0x41, 0x1A, 0xBC, 0xC8, 0x8B, 0x3D, 0x2C, 0xAE, 0xE0, 0xD3 }
PBL_APP_INFO(MY_UUID,
             "Beapoch", "jerith",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);


// TODO: Fix this if and when we get access to timezone data.
#define UTC_OFFSET (60 * 60 * 2)


Window window;

Layer background_layer;
SliderLayer slider_wday_layer;
TextLayer text_date_layer;
TextLayer text_time_layer;
TextLayer text_unix_layer;
TextLayer text_beat_layer;


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
#define ISO_WDAY_HEIGHT 12
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


#define RTOP(rect) (rect.origin.y)
#define RBOTTOM(rect) (rect.origin.y + rect.size.h)
#define RLEFT(rect) (rect.origin.x)
#define RRIGHT(rect) (rect.origin.x + rect.size.w)


void init_text_layer(TextLayer *layer, GRect rect, GTextAlignment align, uint32_t font_res_id) {
    text_layer_init(layer, rect);
    text_layer_set_text_color(layer, TEXT_FG_COLOR);
    text_layer_set_background_color(layer, TEXT_BG_COLOR);
    text_layer_set_text_alignment(layer, align);
    text_layer_set_font(layer, fonts_load_custom_font(resource_get_handle(font_res_id)));
    layer_add_child(&window.layer, &layer->layer);
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


void init_display() {
    layer_init(&background_layer, window.layer.frame);
    background_layer.update_proc = &update_background_callback;
    layer_add_child(&window.layer, &background_layer);

    slider_layer_init(&slider_wday_layer, ISO_WDAY_RECT, 7);
    slider_layer_set_line_color(&slider_wday_layer, GColorClear);
    slider_layer_set_tick_color(&slider_wday_layer, TEXT_FG_COLOR);
    slider_layer_set_indicator_color(&slider_wday_layer, TEXT_FG_COLOR);
    slider_wday_layer.tick_height = 2;
    slider_wday_layer.indicator_radius = 3;
    layer_add_child(&window.layer, &slider_wday_layer.layer);

    init_text_layer(&text_date_layer, ISO_DATE_RECT, GTextAlignmentCenter, RESOURCE_ID_FONT_ISO_DATE_23);
    init_text_layer(&text_time_layer, ISO_TIME_RECT, GTextAlignmentCenter, RESOURCE_ID_FONT_ISO_TIME_31);
    init_text_layer(&text_unix_layer, UNIX_RECT, GTextAlignmentCenter, RESOURCE_ID_FONT_UNIX_TIME_23);
    init_text_layer(&text_beat_layer, BEAT_RECT, GTextAlignmentLeft, RESOURCE_ID_FONT_SWATCH_BEATS_24);
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


time_t calc_unix_seconds(PblTm *tick_time) {
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


void display_time(PblTm *tick_time) {
    // Static, because we pass them to the system.
    static char date_text[] = "9999-99-99";
    static char time_text[] = "99:99:99";
    static char unix_text[] = "0123456789";
    static char beat_text[] = "@1000";

    time_t unix_seconds;

    // Weekday.

    slider_layer_set_position(&slider_wday_layer, (uint8_t)tick_time->tm_wday);

    // Date.

    string_format_time(date_text, sizeof(date_text), "%Y-%m-%d", tick_time);
    text_layer_set_text(&text_date_layer, date_text);

    // Time.

    string_format_time(time_text, sizeof(time_text), "%H:%M:%S", tick_time);
    if (time_text[0] == ' ') {
        time_text[0] = '0';
    }
    text_layer_set_text(&text_time_layer, time_text);

    // Unix timestamp.

    unix_seconds = calc_unix_seconds(tick_time) - UTC_OFFSET;
    strcpy(unix_text, int_to_str(unix_seconds));
    text_layer_set_text(&text_unix_layer, unix_text);

    // Swatch .beats.

    beat_text[1] = '\0';
    strcat(beat_text, int_to_str(calc_swatch_beats(unix_seconds)));
    text_layer_set_text(&text_beat_layer, beat_text);
}


void handle_init(AppContextRef ctx) {
    PblTm init_time;

    (void) ctx;

    window_init(&window, "Beapoch");
    window_stack_push(&window, true /* Animated */);
    window_set_background_color(&window, WINDOW_COLOR);

    resource_init_current_app(&APP_RESOURCES);
    init_display();

    get_time(&init_time);
    display_time(&init_time);
}


void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {
    (void) ctx;

    display_time(t->tick_time);
}


void pbl_main(void *params) {
    PebbleAppHandlers handlers = {
        .init_handler = &handle_init,
        .tick_info = {
            .tick_handler = &handle_second_tick,
            .tick_units = SECOND_UNIT
        }
    };
    app_event_loop(params, &handlers);
}
