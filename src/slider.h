// It's probably bad form to have code in here. Whatever.

#pragma once

#include <pebble.h>

typedef Layer SliderLayer;


typedef struct {
    uint8_t ticks;
    uint8_t position;
    uint8_t tick_height;
    uint8_t indicator_radius;
    GPoint left;
    GPoint right;
    GColor line_color : 2;
    GColor tick_color : 2;
    GColor indicator_color : 2;
} SliderLayerData;


SliderLayer *slider_layer_create(GRect frame, uint8_t ticks);
void slider_layer_destroy(SliderLayer *slider_layer);
void slider_layer_set_ticks(SliderLayer *slider_layer, uint8_t ticks);
void slider_layer_set_position(SliderLayer *slider_layer, uint8_t position);
void slider_layer_set_tick_height(SliderLayer *slider_layer, uint8_t tick_height);
void slider_layer_set_indicator_radius(SliderLayer *slider_layer, uint8_t indicator_radius);
void slider_layer_set_left(SliderLayer *slider_layer, GPoint left);
void slider_layer_set_right(SliderLayer *slider_layer, GPoint right);
void slider_layer_set_line_color(SliderLayer *slider_layer, GColor color);
void slider_layer_set_tick_color(SliderLayer *slider_layer, GColor color);
void slider_layer_set_indicator_color(SliderLayer *slider_layer, GColor color);
