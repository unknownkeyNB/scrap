// Scrap is a project that allows anyone to build software using simple, block based interface.
//
// Copyright (C) 2024-2025 Grisshink
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "scrap_gui.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define SIZING_X(el) (ElementSizing)(el->sizing & 0x0f)
#define SIZING_Y(el) (ElementSizing)((el->sizing >> 4) & 0x0f)
#define SET_SIZING_X(el, size) (el->sizing = (el->sizing & 0xf0) | size)
#define SET_SIZING_Y(el, size) (el->sizing = (el->sizing & 0x0f) | (size << 4))
#define ALIGN(el) (AlignmentType)((el->flags >> 1) & 3)
#define DIRECTION(el) (FlexDirection)(el->flags & 1)
#define SET_ALIGN(el, ali) (el->flags = (el->flags & 1) | (ali << 1))
#define SET_DIRECTION(el, dir) (el->flags = (el->flags & 0xfe) | dir)

void gui_render(Gui* gui, FlexElement* el, int pos_x, int pos_y);

static bool inside_window(Gui* gui, DrawCommand* command) {
    return (command->pos_x + command->width  > 0) && (command->pos_x < gui->win_w) && 
           (command->pos_y + command->height > 0) && (command->pos_y < gui->win_h);
}

static bool mouse_inside(Gui* gui, int x, int y, int w, int h) {
    return ((gui->mouse_x > x) && (gui->mouse_x < x + w) && 
            (gui->mouse_y > y) && (gui->mouse_y < y + h));
}

void gui_init(Gui* gui) {
    gui->measure_text = NULL;
    gui->measure_image = NULL;
    gui->command_stack_len = 0;
    gui->element_stack_len = 0;
    gui->win_w = 0;
    gui->win_h = 0;
    gui->mouse_x = 0;
    gui->mouse_y = 0;
    gui->command_stack_iter = 0;
}

void gui_begin(Gui* gui) {
    gui->command_stack_len = 0;
    gui->command_stack_iter = 0;
    gui->element_stack_len = 0;
    gui->element_ptr_stack_len = 0;
    gui->state_stack_len = 0;
    gui_element_begin(gui);
    gui_set_fixed(gui, gui->win_w, gui->win_h);
}

void gui_end(Gui* gui) {
    gui_element_end(gui);
    gui_render(gui, &gui->element_stack[0], 0, 0);
}

void gui_set_measure_text_func(Gui* gui, MeasureTextFunc measure_text) {
    gui->measure_text = measure_text;
}

void gui_set_measure_image_func(Gui* gui, MeasureImageFunc measure_image) {
    gui->measure_image = measure_image;
}

void gui_update_mouse_pos(Gui* gui, int mouse_x, int mouse_y) {
    gui->mouse_x = mouse_x;
    gui->mouse_y = mouse_y;
}

void gui_update_window_size(Gui* gui, int win_w, int win_h) {
    gui->win_w = win_w;
    gui->win_h = win_h;
}

void gui_render(Gui* gui, FlexElement* el, int pos_x, int pos_y) {
    if (el->handle_hover && mouse_inside(gui, el->x * el->scaling + pos_x, el->y * el->scaling + pos_y, el->w * el->scaling, el->h * el->scaling)) el->handle_hover(el);
    if (el->draw_type != DRAWTYPE_UNKNOWN) {
        DrawCommand* command = &gui->command_stack[gui->command_stack_len++];
        command->pos_x = pos_x + el->x * el->scaling;
        command->pos_y = pos_y + el->y * el->scaling;
        command->width = el->w * el->scaling;
        command->height = el->h * el->scaling;
        command->type = el->draw_type;
        command->color = el->color;
        command->data = el->data;
        if (!inside_window(gui, command)) gui->command_stack_len--;
    }
    
    FlexElement* iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        gui_render(gui, iter, pos_x + el->x * el->scaling, pos_y + el->y * el->scaling);
        iter = iter->next;
    }
}

FlexElement* gui_element_begin(Gui* gui) {
    assert(gui->element_stack_len < ELEMENT_STACK_SIZE);
    assert(gui->element_ptr_stack_len < ELEMENT_STACK_SIZE);

    FlexElement* prev = gui->element_ptr_stack_len > 0 ? gui->element_ptr_stack[gui->element_ptr_stack_len - 1] : NULL;
    FlexElement* el = &gui->element_stack[gui->element_stack_len++];
    gui->element_ptr_stack[gui->element_ptr_stack_len++] = el;
    el->draw_type = DRAWTYPE_UNKNOWN;
    el->is_floating = 0;
    el->x = prev ? prev->cursor_x : 0;
    el->y = prev ? prev->cursor_y : 0;
    el->scaling = prev ? prev->scaling : 1.0;
    el->element_count = 0;
    el->cursor_x = 0;
    el->cursor_y = 0;
    el->sizing = 0; // sizing_x = SIZING_FIT, sizing_y = SIZING_FIT
    el->pad_w = 0;
    el->pad_h = 0;
    el->gap = 0;
    el->w = 0;
    el->h = 0;
    el->flags = 0; // direction = DIRECTION_VERTICAL, align = ALIGN_TOP | ALIGN_LEFT
    el->next = NULL;
    el->handle_hover = NULL;
    el->custom_data = NULL;
    el->custom_state = NULL;
    el->state_len = 0;
    return el;
}

void gui_element_realign(FlexElement* el) {
    if (ALIGN(el) == ALIGN_TOP) return;

    int align_div = ALIGN(el) == ALIGN_CENTER ? 2 : 1;
    FlexElement *iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        if (!iter->is_floating) {
            if (DIRECTION(el) == DIRECTION_VERTICAL) {
                iter->x = (el->w - iter->w) / align_div;
            } else {
                iter->y = (el->h - iter->h) / align_div;
            }
        }
        iter = iter->next;
    }
}

void gui_element_resize(Gui* gui, FlexElement* el, int new_w, int new_h) {
    el->w = MAX(new_w, el->w);
    el->h = MAX(new_h, el->h);

    int left_w = el->w - el->pad_w * 2 + el->gap;
    int left_h = el->h - el->pad_h * 2 + el->gap;
    int grow_elements = 0;

    FlexElement* iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        if (!iter->is_floating) {
            if (DIRECTION(el) == DIRECTION_VERTICAL) {
                if (SIZING_Y(iter) == SIZING_GROW) {
                    grow_elements++;
                } else {
                    left_h -= iter->h;
                }
                left_h -= el->gap;
            } else {
                if (SIZING_X(iter) == SIZING_GROW) {
                    grow_elements++;
                } else {
                    left_w -= iter->w;
                }
                left_w -= el->gap;
            }
        }
        iter = iter->next;
    }

    int cursor_x = el->pad_w;
    int cursor_y = el->pad_h;

    iter = el + 1;
    for (int i = 0; i < el->element_count; i++) {
        if (!iter->is_floating) {
            iter->x = cursor_x;
            iter->y = cursor_y;

            int size_w = iter->w;
            int size_h = iter->h;
            if (DIRECTION(el) == DIRECTION_VERTICAL) {
                if (SIZING_X(iter) == SIZING_GROW) size_w = el->w - el->pad_w * 2;
                if (SIZING_Y(iter) == SIZING_GROW) size_h = left_h / grow_elements;
                if (SIZING_X(iter) == SIZING_GROW || SIZING_Y(iter) == SIZING_GROW) gui_element_resize(gui, iter, size_w, size_h);
                cursor_y += iter->h + el->gap;
            } else {
                if (SIZING_X(iter) == SIZING_GROW) size_w = left_w / grow_elements;
                if (SIZING_Y(iter) == SIZING_GROW) size_h = el->h - el->pad_h * 2;
                if (SIZING_X(iter) == SIZING_GROW || SIZING_Y(iter) == SIZING_GROW) gui_element_resize(gui, iter, size_w, size_h);
                cursor_x += iter->w + el->gap;
            }
        }
        iter = iter->next;
    }

    gui_element_realign(el);
}

void gui_element_advance(Gui* gui, GuiMeasurement ms) {
    FlexElement* el = gui->element_ptr_stack_len > 0 ? gui->element_ptr_stack[gui->element_ptr_stack_len - 1] : NULL;
    if (!el) return;

    if (DIRECTION(el) == DIRECTION_HORIZONTAL) {
        el->cursor_x += ms.w + el->gap;
        if (SIZING_X(el) != SIZING_FIXED) el->w += ms.w + el->gap;
        if (SIZING_Y(el) != SIZING_FIXED) el->h = MAX(el->h, ms.h + el->pad_h * 2);
    } else {
        el->cursor_y += ms.h + el->gap;
        if (SIZING_X(el) != SIZING_FIXED) el->w = MAX(el->w, ms.w + el->pad_w * 2);
        if (SIZING_Y(el) != SIZING_FIXED) el->h += ms.h + el->gap;
    }
}

void gui_element_end(Gui* gui) {
    FlexElement* el = gui->element_ptr_stack[--gui->element_ptr_stack_len];
    FlexElement* prev = gui->element_ptr_stack_len > 0 ? gui->element_ptr_stack[gui->element_ptr_stack_len - 1] : NULL;
    if (DIRECTION(el) == DIRECTION_VERTICAL) {
        el->h -= el->gap;
    } else {
        el->w -= el->gap;
    }
    el->next = &gui->element_stack[gui->element_stack_len];
    if (prev) {
        prev->element_count++;
    }
    gui_element_advance(gui, (GuiMeasurement) { el->w, el->h });
    if ((SIZING_X(el) == SIZING_FIXED && SIZING_Y(el) != SIZING_GROW) || (SIZING_Y(el) == SIZING_FIXED && SIZING_X(el) != SIZING_GROW)) {
        gui_element_resize(gui, el, el->w, el->h);
    } else {
        gui_element_realign(el);
    }
}

void gui_on_hover(Gui* gui, HoverHandler handler) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->handle_hover = handler;
}

void gui_scale_element(Gui* gui, float scaling) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->scaling *= scaling;
}

void* gui_set_state(Gui* gui, void* state, unsigned int state_len) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    if (el->custom_state) return el->custom_state;
    assert(gui->state_stack_len + state_len <= STATE_STACK_SIZE);

    memcpy(&gui->state_stack[gui->state_stack_len], state, state_len);
    el->custom_state = &gui->state_stack[gui->state_stack_len];
    gui->state_stack_len += state_len;
    return el->custom_state;
}

void* gui_get_state(FlexElement* el, unsigned int* state_len) {
    *state_len = el->state_len;
    return el->custom_state;
}

void gui_set_floating(Gui* gui) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->is_floating = 1;
}

void gui_set_position(Gui* gui, int x, int y) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->x = x;
    el->y = y;
}

void gui_set_custom_data(Gui* gui, void* custom_data) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->custom_data = custom_data;
}

void gui_set_fixed(Gui* gui, int w, int h) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->sizing = SIZING_FIXED | (SIZING_FIXED << 4); // This sets both dimensions to SIZING_FIXED
    el->w = w;
    el->h = h;
}

void gui_set_fit(Gui* gui) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->sizing = SIZING_FIT | (SIZING_FIT << 4); // This sets both dimensions to SIZING_FIT
    el->w = 0;
    el->h = 0;
}

void gui_set_padding(Gui* gui, int pad_w, int pad_h) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->pad_w = pad_w;
    el->pad_h = pad_h;
    el->w = MAX(el->w, el->pad_w * 2);
    el->h = MAX(el->h, el->pad_h * 2);
    el->cursor_x = pad_w;
    el->cursor_y = pad_h;
}

void gui_set_gap(Gui* gui, int gap) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->gap = gap;
}

void gui_set_grow(Gui* gui, FlexDirection direction) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    if (direction == DIRECTION_VERTICAL) {
        SET_SIZING_Y(el, SIZING_GROW);
        el->h = 0;
    } else {
        SET_SIZING_X(el, SIZING_GROW);
        el->w = 0;
    }
}

void gui_set_direction(Gui* gui, FlexDirection direction) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    SET_DIRECTION(el, direction);
}

void gui_set_rect(Gui* gui, GuiColor color) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->draw_type = DRAWTYPE_RECT;
    el->color = color;
}

void gui_set_border(Gui* gui, GuiColor color, int border_width) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->draw_type = DRAWTYPE_BORDER;
    el->color = color;
    el->data.border_width = border_width;
}

void gui_set_text(Gui* gui, void* font, const char* text, int size, GuiColor color) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    GuiMeasurement text_size = gui->measure_text(font, text, size);
    el->draw_type = DRAWTYPE_TEXT;
    el->color = color;
    el->data.text.text = text;
    el->data.text.font = font;
    el->w = text_size.w;
    el->h = text_size.h;
}

void gui_set_image(Gui* gui, void* image, int size, GuiColor color) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    GuiMeasurement image_size = gui->measure_image(image, size);
    el->draw_type = DRAWTYPE_IMAGE;
    el->color = color;
    el->data.image = image;
    el->w = image_size.w;
    el->h = image_size.h;
}

void gui_set_align(Gui* gui, AlignmentType align) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    SET_ALIGN(el, align);
}

void gui_text(Gui* gui, void* font, const char* text, int size, GuiColor color) {
    gui_element_begin(gui);
    gui_set_text(gui, font, text, size, color);
    gui_element_end(gui);
}

void gui_image(Gui* gui, void* image, int size, GuiColor color) {
    gui_element_begin(gui);
    gui_set_image(gui, image, size, color);
    gui_element_end(gui);
}

void gui_grow(Gui* gui, FlexDirection direction) {
    gui_element_begin(gui);
    gui_set_grow(gui, direction);
    gui_element_end(gui);
}

void gui_spacer(Gui* gui, int w, int h) {
    gui_element_begin(gui);
    gui_set_min_size(gui, w, h);
    gui_element_end(gui);
}

void gui_set_min_size(Gui* gui, int min_w, int min_h) {
    FlexElement* el = gui->element_ptr_stack[gui->element_ptr_stack_len - 1];
    el->w = MAX(el->w, min_w);
    el->h = MAX(el->h, min_h);
}
