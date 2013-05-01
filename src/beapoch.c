#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0xD8, 0xD3, 0x12, 0xA8, 0xDC, 0xA7, 0x41, 0x1A, 0xBC, 0xC8, 0x8B, 0x3D, 0x2C, 0xAE, 0xE0, 0xD3 }
PBL_APP_INFO(MY_UUID,
             "Beapoch", "jerith",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);


// TODO: Fix this if and when we get access to timezone data.
#define UTC_OFFSET (60 * 60 * 2)


Window window;

TextLayer text_unix_layer;
TextLayer text_time_layer;
TextLayer text_beat_layer;


#define DD_BG_COLOR GColorBlack
#define DD_FG_COLOR GColorWhite


void init_text_layer(TextLayer *layer, int16_t top, uint32_t font_res_id) {
    // We draw all layers starting at the `top' x co-ord and filling the rest of the screen.
    text_layer_init(layer, window.layer.frame);
    text_layer_set_text_color(layer, DD_FG_COLOR);
    text_layer_set_background_color(layer, GColorClear);
    layer_set_frame(&layer->layer, GRect(0, top, 144, 168 - top));
    text_layer_set_text_alignment(layer, GTextAlignmentCenter);
    text_layer_set_font(layer, fonts_load_custom_font(resource_get_handle(font_res_id)));
    layer_add_child(&window.layer, &layer->layer);
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


void display_time(PblTm *tick_time) {
    // Static, because we pass them to the system.
    static char time_text[] = "00:00:00";
    static char unix_text[] = "0123456789";
    static char beat_text[] = "@1000";

    char *time_format;
    time_t unix_seconds;

    // Calculate standard time.

    if (clock_is_24h_style()) {
        time_format = "%H:%M:%S";
    } else {
        time_format = "%I:%M:%S";
    }

    string_format_time(time_text, sizeof(time_text), time_format, tick_time);

    if (!clock_is_24h_style() && (time_text[0] == '0')) {
        time_text[0] = ' ';
    }

    // Calculate unix time.

    unix_seconds = calc_unix_seconds(tick_time) - UTC_OFFSET;
    strcpy(unix_text, int_to_str(unix_seconds));

    // Calculate .beats.

    beat_text[1] = '\0';
    strcat(beat_text, int_to_str((((unix_seconds + 3600) % 86400) * 1000) / 86400));

    // Update display.

    text_layer_set_text(&text_time_layer, time_text);
    text_layer_set_text(&text_unix_layer, unix_text);
    text_layer_set_text(&text_beat_layer, beat_text);
}


void handle_init(AppContextRef ctx) {
    PblTm init_time;

    (void) ctx;

    window_init(&window, "Beapoch");
    window_stack_push(&window, true /* Animated */);
    window_set_background_color(&window, GColorBlack);

    resource_init_current_app(&APP_RESOURCES);

    init_text_layer(&text_unix_layer, 4, RESOURCE_ID_FONT_ROBOTO_CONDENSED_19);
    init_text_layer(&text_time_layer, 64, RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_32);
    init_text_layer(&text_beat_layer, 126, RESOURCE_ID_FONT_ROBOTO_CONDENSED_19);

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
