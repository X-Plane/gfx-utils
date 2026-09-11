/*===--------------------------------------------------------------------------------------------===
 * gfx.c
 *
 * Created by Amy Alex Parent <amy@amyparent.com> on 24/06/2026
 * Copyright (c) 2026 Laminar Research. All rights reserved
 *
 * Licensed under the MIT License
 *===--------------------------------------------------------------------------------------------===
*/
#include "gfx.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <XPLMPanelGraphics.h>

#define RAD2DEG(x)              ((x) * 180.f/M_PI)
#define DEG2RAD(x)              ((x) * M_PI/180.f)


#define GFX_MAX_TTF_FONTS       8
#define GFX_MAX_BITMAP_FONTS    8
#define GFX_MAX_STATE_STACK     32
#define GFX_DEF_VERTEX_CAP      32
#define GFX_ARC_SEG_LEN         30
#define GFX_ARC_CURV_DEG        DEG2RAD(5)


typedef struct gfx_state_t {
    uint32_t        color;
    float           line_width;
    gfx_line_cap_t  line_cap;
    float           font_size;
    int32_t         font_id;
    int32_t         font_id_bitmap;
} gfx_state_t;

typedef struct gfx_bitmap_font_t {
    char                first_char;
    int32_t             char_count;
    int32_t             start_idx;
    bool                short_dot;
    int                 char_w;
    int                 char_h;
} gfx_bitmap_font_t;

typedef enum gfx_cmd_t {
    GFX_CMD_VTX,
    GFX_CMD_CLOSE,
    GFX_CMD_MOVE,
} gfx_cmd_t;

struct gfx_ctx_t {
    gfx_state_t         saved_state[GFX_MAX_STATE_STACK];
    gfx_state_t         *state;
    
    bool                complex;
    bool                closed;
    XPLMVertex_t        *vtx;
    gfx_cmd_t           *cmd;
    uint16_t            vtx_count;
    uint16_t            vtx_cap;
    
    int                 subpath_start;
};

typedef struct {
    bool                inited;
    XPLMTextureAtlasRef atlas;
    bool                baked;
    gfx_bitmap_font_t   bitmap_fonts[GFX_MAX_BITMAP_FONTS];
    uint16_t            bitmap_font_count;
    XPLMFontHandle      ttf_fonts[GFX_MAX_TTF_FONTS];
    uint16_t            ttf_font_count;
} gfx_shared_t;


// Shared state! This isn't ideal, but it's better than loading the same textures in VRAM for
// each context.



static gfx_shared_t shared = { .inited = false };


void gfx_init_shared() {
    assert(!shared.inited);
    shared.atlas = XPLMCreateTextureAtlas();
    shared.inited = true;
    shared.ttf_font_count = 0;
    shared.bitmap_font_count = 0;
}

void gfx_bake_shared() {
    assert(shared.inited);
    assert(!shared.baked);
    XPLMTextureAtlasBake(shared.atlas);
    for(int32_t i = 0; i < shared.bitmap_font_count; ++i) {
        gfx_bitmap_font_t *font = &shared.bitmap_fonts[i];
        font->char_w = XPLMTextureAtlasGetImageWidth(shared.atlas, (int)font->start_idx);
        font->char_h = XPLMTextureAtlasGetImageHeight(shared.atlas, (int)font->start_idx);
    }
    shared.baked = true;
}

void gfx_fini_shared() {
    assert(shared.inited);
    XPLMDestroyTextureAtlas(shared.atlas);
    for(int32_t i = 0; i < shared.ttf_font_count; ++i) {
        XPLMDestroyFont(shared.ttf_fonts[i]);
    }
    shared.ttf_font_count = 0;
    shared.bitmap_font_count = 0;
    shared.atlas = NULL;
    shared.baked = false;
    shared.inited = false;
}

int32_t gfx_load_tex(const char *path) {
    assert(shared.inited);
    assert(shared.atlas != NULL);
    return XPLMTextureAtlasAddImageFile(shared.atlas, path);
}

int32_t gfx_load_tex_atlas(const char *path, int col, int row, int *w, int *h) {
    assert(shared.inited);
    assert(shared.atlas != NULL);
    int idx = XPLMTextureAtlasAddImageFileSet(shared.atlas, path, col, row);
    
    if(w != NULL) {
        *w = XPLMTextureAtlasGetImageWidth(shared.atlas, idx);
    }
    if(h != NULL) {
        *h = XPLMTextureAtlasGetImageHeight(shared.atlas, idx);
    }
    
    return idx;
}

int32_t gfx_load_bitmap_font(const char *path, const gfx_bitmap_font_desc_t *desc) {
    assert(shared.inited);
    assert(!shared.baked);
    assert(path != NULL);
    assert(desc != NULL);
    assert(shared.bitmap_font_count < GFX_MAX_BITMAP_FONTS);
    assert(desc->char_count > 0);
    assert(desc->char_count + desc->first_char < 256);
    
    int32_t id = shared.bitmap_font_count++;
    gfx_bitmap_font_t *font = &shared.bitmap_fonts[id];
    
    font->first_char = desc->first_char;
    font->char_count = desc->char_count;
    font->short_dot = desc->short_dot;
    font->start_idx = XPLMTextureAtlasAddImageFileSet(
        shared.atlas,
        path,
        GFX_BITMAP_FONT_COLS,
        GFX_BITMAP_FONT_ROWS
    );
    return id;
}

int32_t gfx_load_ttf_font(const char *path, bool all_utf8) {
    assert(shared.inited);
    assert(!shared.baked);
    assert(path != NULL);
    assert(shared.ttf_font_count < GFX_MAX_TTF_FONTS);
    
    int32_t id = shared.ttf_font_count++;
    XPLMFontHandle font = XPLMCreateFont(all_utf8 ? xplm_CharSetUnicode : xplm_CharSetASCII);
    XPLMFontAddFace(font, path);
    shared.ttf_fonts[id] = font;
    return id;
}

uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return gfx_rgba(r, g, b, 255);
}

uint32_t gfx_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return XPLMMakeColor(r/255.f, g/255.f, b/255.f, a/255.f);
}

uint32_t gfx_rgbf(float r, float g, float b) {
    return gfx_rgbaf(r, g, b, 1.f);
}
uint32_t gfx_rgbaf(float r, float g, float b, float a) {
    return XPLMMakeColor(r, g, b, a);
}

uint32_t gfx_color_mult(uint32_t color, float m) {
    /*
    return (ialpha << 24) | (iblue << 16) | (igreen << 8) | ired;
    */
    uint8_t alpha = (color >> 24) & 0xff;
    uint32_t color_no_alpha = color & ~(0xff << 24);
    return color_no_alpha | ((uint32_t)(alpha * m) << 24);
}

static void gfx_init_state(gfx_state_t *state) {
    state->line_cap = GFX_LINE_CAP_SQUARE;
    state->color = gfx_rgbf(1.f, 1.f, 1.f);
    state->font_id = 0;
    state->font_id_bitmap = -1;
    state->font_size = 10.f;
}

static void gfx_vtx_ensure(gfx_ctx_t *ctx, uint16_t add) {
    assert(ctx != NULL);
    if(ctx->vtx_count + add < ctx->vtx_cap) {
        return;
    }
    while(ctx->vtx_count + add >= ctx->vtx_cap) {
        ctx->vtx_cap *= 2;
    }
    ctx->vtx = realloc(ctx->vtx, ctx->vtx_cap * sizeof(*ctx->vtx));
    ctx->cmd = realloc(ctx->cmd, ctx->vtx_cap * sizeof(*ctx->cmd));
    assert(ctx->vtx != NULL);
    assert(ctx->cmd != NULL);
}

gfx_ctx_t *gfx_new() {
    gfx_ctx_t *ctx = calloc(1, sizeof(*ctx));
    assert(ctx != NULL);
    
    ctx->vtx_count = 0;
    ctx->vtx_cap = GFX_DEF_VERTEX_CAP;
    ctx->vtx = calloc(ctx->vtx_cap, sizeof(*ctx->vtx));
    assert(ctx->vtx != NULL);
    ctx->cmd = calloc(ctx->vtx_cap, sizeof(*ctx->cmd));
    ctx->state = &ctx->saved_state[0];
    ctx->complex = false;
    ctx->subpath_start = 0;
    gfx_init_state(ctx->state);
    
    return ctx;
}

void gfx_destroy(gfx_ctx_t *ctx) {
    assert(ctx != NULL);
    if(ctx->vtx != NULL) {
        free(ctx->vtx);
    }
    if(ctx->cmd != NULL) {
        free(ctx->cmd);
    }
    // XPLMDestroyTextureAtlas(ctx->atlas);
    memset(ctx, 0, sizeof(*ctx));
    free(ctx);
}

static void gfx_clear_path(gfx_ctx_t *ctx) {
    ctx->subpath_start = 0;
    ctx->vtx_count = 0;
    ctx->closed = false;
    ctx->complex = false;
}

void gfx_begin_frame(gfx_ctx_t *ctx) {
    gfx_reset(ctx);
    ctx->state = &ctx->saved_state[0];
    ctx->subpath_start = 0;
    gfx_init_state(ctx->state);
    gfx_save(ctx);
}

void gfx_end_frame(gfx_ctx_t *ctx) {
    gfx_restore(ctx);
    assert(ctx->state == &ctx->saved_state[0]);
}

void gfx_set_color(gfx_ctx_t *ctx, uint32_t color) {
    assert(ctx != NULL);
    ctx->state->color = color;
}

void gfx_set_line_width(gfx_ctx_t *ctx, float width) {
    assert(ctx != NULL);
    assert(width > 0.f);
    ctx->state->line_width = width;
}

void gfx_set_line_cap(gfx_ctx_t *ctx, gfx_line_cap_t cap) {
    assert(ctx != NULL);
    ctx->state->line_cap = cap;
    XPLMSetLineCap((XPLMLineCap_t)cap);
}

static const XPLMVertex_t *find_next_end(const XPLMVertex_t *start, const XPLMVertex_t *stop) {
    for(const XPLMVertex_t *vtx = start; vtx != stop; ++vtx) {
        if(isnan(vtx->x) && isnan(vtx->y)) {
            return vtx;
        }
    }
    return stop;
}

static const XPLMVertex_t *find_next_start(const XPLMVertex_t *end, const XPLMVertex_t *stop) {
    if(end >= stop) {
        return stop;
    }
    return end + 1;
}

static uint16_t gfx_find_subpath_end(gfx_ctx_t *ctx, uint16_t start) {
    for(uint16_t i = start+1; i < ctx->vtx_count; ++i) {
        if(ctx->cmd[i] == GFX_CMD_MOVE) {
            return i;
        }
    }
    return ctx->vtx_count;
}

static uint16_t gfx_find_subpath_start(gfx_ctx_t *ctx, uint16_t end) {
    for(uint16_t i = end; i < ctx->vtx_count; ++i) {
        if(ctx->cmd[i] == GFX_CMD_MOVE) {
            return i;
        }
    }
    return ctx->vtx_count;
}

static void gfx_stroke_impl(gfx_ctx_t *ctx, bool clr) {
    if(ctx->vtx_count >= 2) {
        uint16_t start = 0;
        uint16_t stop = ctx->vtx_count;
        uint16_t end = gfx_find_subpath_end(ctx, start);
        
        while(start != stop) {
            bool is_closed =
                end != stop
                && end != start
                && ctx->cmd[end-1] == GFX_CMD_CLOSE;
            uint16_t count = end - start;
            if(is_closed && count >= 4) {
                XPLMLineLoopWithWidth(
                    ctx->state->color,
                    ctx->state->line_width,
                    &ctx->vtx[start],
                    count-1
                    // -1 so we don't get double points at the close, XPLM seems to corrupt lines in that case
                );
            } else if(!is_closed && count >= 2) {
                XPLMLineStripWithWidth(
                    ctx->state->color,
                    ctx->state->line_width,
                    &ctx->vtx[start],
                    count
                );
            }
            
            start = gfx_find_subpath_start(ctx, end);
            end = gfx_find_subpath_end(ctx, start);
        }
    }
    if(clr) {
        gfx_clear_path(ctx);
    }
}

static void gfx_fill_impl(gfx_ctx_t *ctx, bool clr) {
    if(ctx->vtx_count >= 2) {
        uint16_t start = 0;
        uint16_t stop = ctx->vtx_count;
        uint16_t end = gfx_find_subpath_end(ctx, start);
        
        while(start != stop) {
            uint16_t count = end - start;
            if(count >= 3) {
                XPLMPolygon(
                    ctx->state->color,
                    &ctx->vtx[start],
                    count
                );
            }
            start = gfx_find_subpath_start(ctx, end);
            end = gfx_find_subpath_end(ctx, start);
        }
    }
    if(clr) {
        gfx_clear_path(ctx);
    }
}

static void gfx_clip_impl(gfx_ctx_t *ctx, bool clr) {
    XPLMBeginSetupStencilMask(0x01, 0x01);
    gfx_fill_impl(ctx, clr);
    XPLMEndSetupStencilMask();
    XPLMUseStencilMask(0x01, 0x01);
}

void gfx_stroke(gfx_ctx_t *ctx) {
    gfx_stroke_impl(ctx, true);
    gfx_clear_path(ctx);
}

void gfx_stroke_fast(gfx_ctx_t *ctx) {
    if(ctx->vtx_count > 2) {
        XPLMLinesWithWidth(ctx->state->color, ctx->state->line_width, ctx->vtx, ctx->vtx_count);
    }
    ctx->vtx_count = 0;
    ctx->closed = false;
    ctx->complex = false;
}

void gfx_stroke_preserve(gfx_ctx_t *ctx) {
    gfx_stroke_impl(ctx, false);
}

void gfx_fill(gfx_ctx_t *ctx) {
    gfx_fill_impl(ctx, true);
}

void gfx_fill_preserve(gfx_ctx_t *ctx) {
    gfx_fill_impl(ctx, false);
}

void gfx_clip(gfx_ctx_t *ctx) {
    assert(ctx != NULL);
    gfx_clip_impl(ctx, true);
}

void gfx_clip_preserve(gfx_ctx_t *ctx) {
    assert(ctx != NULL);
    gfx_clip_impl(ctx, false);
}

void gfx_clip_reset(gfx_ctx_t *ctx) {
    assert(ctx != NULL);
    XPLMUseStencilMask(0, 0);
    XPLMClearStencilMask();
}

static void gfx_add_vtx(gfx_ctx_t *ctx, float x, float y) {
    gfx_vtx_ensure(ctx, 1);
    ctx->cmd[ctx->vtx_count] = GFX_CMD_VTX;
    XPLMVertex_t *vtx = &ctx->vtx[ctx->vtx_count++];
    vtx->x = x;
    vtx->y = y;
}

static void gfx_add_move(gfx_ctx_t *ctx, float x, float y) {
    gfx_vtx_ensure(ctx, 1);
    ctx->cmd[ctx->vtx_count] = GFX_CMD_MOVE;
    XPLMVertex_t *vtx = &ctx->vtx[ctx->vtx_count++];
    vtx->x = x;
    vtx->y = y;
}

static void gfx_add_close(gfx_ctx_t *ctx, float x, float y) {
    gfx_vtx_ensure(ctx, 1);
    ctx->cmd[ctx->vtx_count] = GFX_CMD_CLOSE;
    XPLMVertex_t *vtx = &ctx->vtx[ctx->vtx_count++];
    vtx->x = x;
    vtx->y = y;
}

void gfx_move_to(gfx_ctx_t *ctx, float x, float y) {
    assert(ctx != NULL);
    ctx->subpath_start = ctx->vtx_count;
    gfx_add_move(ctx, x, y);
}

void gfx_line_to(gfx_ctx_t *ctx, float x, float y) {
    assert(ctx != NULL);
    if(ctx->vtx_count == 0) {
        gfx_move_to(ctx, x, y);
    } else {
        gfx_add_vtx(ctx, x, y);
    }    
}

void gfx_curve_to(gfx_ctx_t *ctx, float x1, float y1, float x2, float y2, float x3, float y3) {
    assert(ctx != NULL);
    if(ctx->vtx_count == 0) {
        gfx_move_to(ctx, x1, y1);
    }
    
    assert(ctx->vtx_count > 0);
    float x0 = ctx->vtx[ctx->vtx_count-1].x;
    float y0 = ctx->vtx[ctx->vtx_count-1].y;
    
    float dist_x = (x3 - x0);
    float dist_y = (y3 - y0);
    float dist = sqrtf(dist_x*dist_x + dist_y*dist_y);
    int count = ceilf(dist / 5.f);
    
    for(int i = 1; i <= count; ++i) {
        float t = (float)i / (float)count;
        float tm1 = (1.f - t);
        float t_2 = powf(t, 2.f);
        float t_3 = powf(t, 3.f);
        float tm1_2 = powf(tm1, 2.f);
        float tm1_3 = powf(tm1, 3.f);
        float x = tm1_3*x0 + 3.f*tm1_2*t*x1 + 3.f*tm1*t_2*x2 + t_3*x3;
        float y = tm1_3*y0 + 3.f*tm1_2*t*y1 + 3.f*tm1*t_2*y2 + t_3*y3;
        gfx_line_to(ctx, x, y);
    }
}

static float norm_ang(float ang) {
    while(ang < 0) {
        ang += 2*M_PI;
    }
    while(ang >= 2*M_PI) {
        ang -= 2*M_PI;
    }
    return ang;
}

void gfx_arc(gfx_ctx_t *ctx, float cx, float cy, float rad, float a0, float a1) {
    assert(ctx != NULL);
    assert(rad > 0.f);
    float angle = a1 - a0;
    
    while(angle < 0) {
        angle += 2*M_PI;
    }
    
    float arc_len = angle * rad;
    int seg_count = ceil(angle / GFX_ARC_CURV_DEG);
    float da = fabsf(angle) / seg_count;
    
    gfx_vtx_ensure(ctx, seg_count+1);
    for(int i = 0; i < seg_count + 1; ++i) {
        float a = a0 + i * da;
        float x = cx + rad * cosf(a);
        float y = cy + rad * sinf(a);
        gfx_add_vtx(ctx, x, y);
    }
}

void gfx_arc_negative(gfx_ctx_t *ctx, float cx, float cy, float rad, float a0, float a1) {
    assert(ctx != NULL);
    assert(rad > 0.f);
    float angle = a0 - a1;
    
    while(angle < 0) {
        angle += 2*M_PI;
    }
    
    float arc_len = angle * rad;
    int seg_count = ceil(angle / GFX_ARC_CURV_DEG);
    float da = angle / seg_count;
    
    gfx_vtx_ensure(ctx, seg_count+1);
    for(int i = 0; i < seg_count + 1; ++i) {
        float a = a0 - i * da;
        float x = cx + rad * cosf(a);
        float y = cy + rad * sinf(a);
        gfx_add_vtx(ctx, x, y);
    }
}

void gfx_rectangle(gfx_ctx_t *ctx, float x, float y, float w, float h) {
    assert(ctx != NULL);
    assert(w >= 0.f);
    assert(h >= 0.f);
    
    gfx_vtx_ensure(ctx, 4);
    gfx_add_move(ctx, x, y);
    gfx_add_vtx(ctx, x, y + h);
    gfx_add_vtx(ctx, x + w, y + h);
    gfx_add_vtx(ctx, x + w, y);
    gfx_close_path(ctx);
}

void gfx_close_path(gfx_ctx_t *ctx) {
    assert(ctx != NULL);
    if(ctx->vtx_count < ctx->subpath_start + 2) {
        return;
    }
    const XPLMVertex_t *start = &ctx->vtx[ctx->subpath_start];
    gfx_add_close(ctx, start->x, start->y);
    gfx_add_move(ctx, start->x, start->y);
}


void gfx_tex(gfx_ctx_t *ctx, int img, float x, float y) {
    assert(ctx != NULL);
    XPLMTextureAtlasDrawScaled(shared.atlas, img, ctx->state->color, x, y, 0, 0, 1.f, 1.f, 0.f);
    // XPLMTextureAtlasDrawAt(ctx->atlas, img, ctx->state->color, x, y);
}

void gfx_translate(gfx_ctx_t *ctx, float x, float y) {
    assert(ctx != NULL);
    XPLMTransformTranslate(x, y);
}

void gfx_rotate(gfx_ctx_t *ctx, float angle) {
    assert(ctx != NULL);
    XPLMTransformRotate(0, 0, -RAD2DEG(angle));
}

void gfx_scale(gfx_ctx_t *ctx, float x, float y) {
    assert(ctx != NULL);
    XPLMTransformScale(x, y);
}

void gfx_reset(gfx_ctx_t *ctx) {
    assert(ctx != NULL);
    ctx->state = ctx->saved_state;
    gfx_init_state(ctx->state);
    ctx->complex = false;
    ctx->closed = false;
}

void gfx_save(gfx_ctx_t *ctx) {
    assert(ctx != NULL);
    assert(ctx->state + 1 < &ctx->saved_state[GFX_MAX_STATE_STACK]);
    ctx->state += 1;
    memcpy(ctx->state, ctx->state - 1, sizeof(*ctx->state));
    XPLMTransformPush();
}

void gfx_restore(gfx_ctx_t *ctx) {
    assert(ctx != NULL);
    assert(ctx->state > &ctx->saved_state[0]);
    ctx->state -= 1;
    XPLMTransformPop();
    XPLMSetLineCap((XPLMLineCap_t)ctx->state->line_cap);
}


void gfx_set_bitmap_font(gfx_ctx_t *ctx, int32_t font_id) {
    assert(ctx != NULL);
    assert(font_id >= 0 && font_id < shared.bitmap_font_count);
    ctx->state->font_id_bitmap = font_id;
}

void gfx_set_font_face(gfx_ctx_t *ctx, int32_t font_id) {
    assert(ctx != NULL);
    assert(font_id >= 0 && font_id < shared.ttf_font_count);
    ctx->state->font_id = font_id;
}


void gfx_set_font_size(gfx_ctx_t *ctx, float font_size) {
    assert(ctx != NULL);
    assert(font_size >= 1.f);
    ctx->state->font_size = font_size;
}

void gfx_draw_text(
    gfx_ctx_t *ctx,
    float x,
    float y,
    gfx_text_align_t align,
    const char *fmt,
    ...
) {
    assert(ctx != NULL);
    assert(ctx->state->font_id_bitmap < shared.bitmap_font_count);
    
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    
    size_t len = vsnprintf(NULL, 0, fmt, args)+1;
    char *text = malloc(len);
    assert(text != NULL);
    
    vsnprintf(text, len, fmt, args2);
    
    va_end(args);
    va_end(args2);
    
    XPLMFontDrawString(
        shared.ttf_fonts[ctx->state->font_id],
        ctx->state->color,
        ctx->state->font_size,
        x, y,
        text,
        align
    );
    free(text);
}


static int gfx_get_glyph_id(const gfx_bitmap_font_t *font, char c) {
    if(c < ' ' || c > 'Z') {
        return -1;
    }
    if(c < font->first_char || c >= font->first_char + font->char_count) {
        return -1;
    }
    return font->start_idx + (c - font->first_char);
}

static float calc_width(const gfx_bitmap_font_t *font, const char *str, size_t len) {
    float w = 0.f;
    for(size_t i = 0; i < len; ++i) {
        w += str[i] == '.' ? font->char_w / 2 : font->char_w;
    }
    return w;
}

void gfx_draw_text_bitmap_ex(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float align,
    float extra_pad,
    const char *fmt,
    ...
) {
    assert(ctx != NULL);
    assert(ctx->state->font_id_bitmap < shared.bitmap_font_count);
    
    va_list args;
    va_start(args, fmt);
    
    char str[16];
    size_t len = vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);
    
    if(len == 0) {
        return;
    }
    
    const gfx_bitmap_font_t *font = &shared.bitmap_fonts[ctx->state->font_id_bitmap];
    
    float width = (len * font->char_w) + (len - 1) * extra_pad;
    float x_offset = -ceilf(align * width);
    
    float c_x = x + x_offset;
    float c_y = y;
    for(size_t i = 0; i < len; ++i) {
        int idx = gfx_get_glyph_id(font, str[i]);
        if(idx >= 0) {
            gfx_tex(ctx, idx, c_x, c_y);
        }
        c_x += font->char_w + extra_pad;
    }
}

void gfx_draw_text_bitmap(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float align,
    const char *fmt,
    ...
) {
    assert(ctx != NULL);
    assert(ctx->state->font_id_bitmap < shared.bitmap_font_count);
    
    va_list args;
    va_start(args, fmt);
    
    char str[32];
    size_t len = vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);
    
    if(len == 0) {
        return;
    }
    
    const gfx_bitmap_font_t *font = &shared.bitmap_fonts[ctx->state->font_id_bitmap];
    
    float width = calc_width(font, str, len);
    float x_offset = -ceilf(align * width);
    
    float c_x = x + x_offset;
    float c_y = y;
    for(size_t i = 0; i < len; ++i) {
        int idx = gfx_get_glyph_id(font, str[i]);
        if(idx >= 0) {
            gfx_tex(ctx, idx, c_x, c_y);
        }
        if(str[i] == '.') {
            c_x += (float)(font->char_w/2.f);
        } else {
            c_x += font->char_w;
        }
    }
}

void gfx_draw_text_bitmap_inv(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float align,
    const char *fmt,
    ...
) {
    assert(ctx != NULL);
    assert(ctx->state->font_id_bitmap < shared.bitmap_font_count);
    
    va_list args;
    va_start(args, fmt);
    
    char str[32];
    size_t len = vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);
    
    if(len == 0) {
        return;
    }
    
    const gfx_bitmap_font_t *font = &shared.bitmap_fonts[ctx->state->font_id_bitmap];
    
    float height = font->char_h;
    float width = calc_width(font, str, len);
    float x_offset = -ceilf(align * width);
    
    float c_x = x + x_offset;
    float c_y = y;
    
    gfx_rectangle(ctx, c_x, c_y, width, height);
    gfx_fill(ctx);
    
    gfx_save(ctx);
    gfx_set_color(ctx, gfx_rgbf(0, 0, 0));
    for(size_t i = 0; i < len; ++i) {
        int idx = gfx_get_glyph_id(font, str[i]);
        if(idx >= 0) {
            gfx_tex(ctx, idx, c_x, c_y);
        }
        if(str[i] == '.') {
            c_x += (float)(font->char_w/2.f);
        } else {
            c_x += font->char_w;
        }
    }
    gfx_restore(ctx);
}

void gfx_draw_text_bitmap_v(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float align,
    const char *fmt,
    ...
) {
    assert(ctx != NULL);
    assert(ctx->state->font_id_bitmap < shared.bitmap_font_count);
    va_list args;
    va_start(args, fmt);
    
    char str[32];
    size_t len = vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);
    
    if(len == 0) {
        return;
    }
    
    const gfx_bitmap_font_t *font = &shared.bitmap_fonts[ctx->state->font_id_bitmap];
    
    float height = font->char_h * len;
    float y_offset = ceilf(align * height);
    
    float c_x = x;
    float c_y = y + y_offset;
    for(size_t i = 0; i < len; ++i) {
        int idx = gfx_get_glyph_id(font, str[i]);
        if(idx >= 0) {
            gfx_tex(ctx, idx, c_x, c_y);
        }
        c_y -= font->char_h;
    }
}

