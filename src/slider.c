
#include <pebble.h>

#include "slider.h"


static void slider_layer_update(SliderLayer *slider_layer, GContext *gctx) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    uint8_t x, y, w;

    if (data->line_color != GColorClear) {
        graphics_context_set_stroke_color(gctx, data->line_color);
        graphics_draw_line(gctx, data->left, data->right);
    }

    graphics_context_set_stroke_color(gctx, data->tick_color);
    y = data->left.y;
    w = data->right.x - data->left.x;
    for (int i = 0; i < data->ticks; ++i) {
        x = data->left.x + (w * i + w / 2) / (data->ticks);
        if (data->tick_color != GColorClear) {
            graphics_draw_line(gctx, GPoint(x, y - data->tick_height), GPoint(x, y + data->tick_height + 1));
        }

        if ((i == data->position) && (data->indicator_color != GColorClear)) {
            graphics_context_set_stroke_color(gctx, data->indicator_color);
            graphics_context_set_fill_color(gctx, data->indicator_color);
            graphics_fill_circle(gctx, GPoint(x, y), data->indicator_radius);
            graphics_context_set_stroke_color(gctx, data->tick_color);
        }
    }
}


SliderLayer *slider_layer_create(GRect frame, uint8_t ticks) {
    SliderLayer *slider_layer = layer_create_with_data(frame, sizeof(SliderLayerData));
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);

    data->ticks = ticks;
    data->position = 0;
    data->tick_height = 5;
    data->indicator_radius = 5;
    data->left = GPoint(0, frame.size.h / 2);
    data->right = GPoint(frame.size.w, frame.size.h / 2);
    data->line_color = GColorBlack;
    data->tick_color = GColorBlack;
    data->indicator_color = GColorBlack;

    layer_set_update_proc(slider_layer, slider_layer_update);
    layer_mark_dirty(slider_layer);
    return slider_layer;
}


void slider_layer_destroy(SliderLayer *slider_layer) {
    layer_destroy(slider_layer);
}


void slider_layer_set_ticks(SliderLayer *slider_layer, uint8_t ticks) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->ticks = ticks;
    layer_mark_dirty(slider_layer);
}


void slider_layer_set_position(SliderLayer *slider_layer, uint8_t position) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->position = position;
    layer_mark_dirty(slider_layer);
}


void slider_layer_set_tick_height(SliderLayer *slider_layer, uint8_t tick_height) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->tick_height = tick_height;
    layer_mark_dirty(slider_layer);
}


void slider_layer_set_indicator_radius(SliderLayer *slider_layer, uint8_t indicator_radius) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->indicator_radius = indicator_radius;
    layer_mark_dirty(slider_layer);
}


void slider_layer_set_left(SliderLayer *slider_layer, GPoint left) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->left = left;
    layer_mark_dirty(slider_layer);
}


void slider_layer_set_right(SliderLayer *slider_layer, GPoint right) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->right = right;
    layer_mark_dirty(slider_layer);
}


void slider_layer_set_line_color(SliderLayer *slider_layer, GColor color) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->line_color = color;
    layer_mark_dirty(slider_layer);
}


void slider_layer_set_tick_color(SliderLayer *slider_layer, GColor color) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->tick_color = color;
    layer_mark_dirty(slider_layer);
}


void slider_layer_set_indicator_color(SliderLayer *slider_layer, GColor color) {
    SliderLayerData *data = (SliderLayerData *)layer_get_data(slider_layer);
    data->indicator_color = color;
    layer_mark_dirty(slider_layer);
}
