// It's probably bad form to have code in here. Whatever.

#pragma once

#include "pebble_os.h"

typedef struct SliderLayer {
    Layer layer;
    uint8_t ticks;
    uint8_t position;
    uint8_t tick_height;
    uint8_t indicator_radius;
    GPoint left;
    GPoint right;
    GColor line_color : 2;
    GColor tick_color : 2;
    GColor indicator_color : 2;
} SliderLayer;


void slider_layer_update_proc(SliderLayer *slider_layer, GContext *gctx) {
    uint8_t x, y, w;

    graphics_context_set_stroke_color(gctx, slider_layer->line_color);
    graphics_draw_line(gctx, slider_layer->left, slider_layer->right);

    graphics_context_set_stroke_color(gctx, slider_layer->tick_color);
    y = slider_layer->left.y;
    w = slider_layer->right.x - slider_layer->left.x;
    for (int i = 0; i < slider_layer->ticks; ++i) {
        x = slider_layer->left.x + (w * i + w / 2) / (slider_layer->ticks);
        graphics_draw_line(gctx, GPoint(x, y - slider_layer->tick_height), GPoint(x, y + slider_layer->tick_height));

        if (i == slider_layer->position) {
            graphics_context_set_stroke_color(gctx, slider_layer->indicator_color);
            graphics_context_set_fill_color(gctx, slider_layer->indicator_color);
            graphics_fill_circle(gctx, GPoint(x, y), slider_layer->indicator_radius);
            graphics_context_set_stroke_color(gctx, slider_layer->tick_color);
        }
    }
}


void slider_layer_init(SliderLayer *slider_layer, GRect frame, uint8_t ticks) {
    layer_init(&slider_layer->layer, frame);
    slider_layer->layer.update_proc = (LayerUpdateProc)slider_layer_update_proc;
    slider_layer->ticks = ticks;
    slider_layer->position = 0;
    slider_layer->tick_height = 5;
    slider_layer->indicator_radius = 5;
    slider_layer->left = GPoint(0, frame.size.h / 2);
    slider_layer->right = GPoint(frame.size.w, frame.size.h / 2);
    slider_layer->line_color = GColorBlack;
    slider_layer->tick_color = GColorBlack;
    slider_layer->indicator_color = GColorBlack;
}


void slider_layer_set_line_color(SliderLayer *slider_layer, GColor color) {
    slider_layer->line_color = color;
}


void slider_layer_set_tick_color(SliderLayer *slider_layer, GColor color) {
    slider_layer->tick_color = color;
}


void slider_layer_set_indicator_color(SliderLayer *slider_layer, GColor color) {
    slider_layer->indicator_color = color;
}


void slider_layer_set_position(SliderLayer *slider_layer, uint8_t position) {
    slider_layer->position = position;
    layer_mark_dirty(&slider_layer->layer);
}
