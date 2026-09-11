/*===--------------------------------------------------------------------------------------------===
 * gfx.h
 *
 * Created by Amy Alex Parent <amy@amyparent.com> on 23/06/2026
 * Copyright (c) 2026 Laminar Research. All rights reserved
 *
 * Licensed under the MIT License
 *===--------------------------------------------------------------------------------------------===
*/
#ifndef _LAMINAR_GFX_H_
#define _LAMINAR_GFX_H_

#include <XPLMPanelGraphics.h>
#include <stdbool.h>
#include <stdint.h>

#define GFX_BITMAP_FONT_COLS    16
#define GFX_BITMAP_FONT_ROWS    4

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_ctx_t gfx_ctx_t;

/**
 * Initialises shared resources used by the GFX layer. Call once at plugin start.
 */
void gfx_init_shared();

/**
 * Bakes any texture atlases needed by GFX objects. Call once all your GFX contexts
 * are done with initialisation, and you do not need to create any more resources.
 */
void gfx_bake_shared();

/**
 * Frees any memory used by GFX shared resources. Call once at plugin teardown.
 */
void gfx_fini_shared();

/**
 * Creates a color object from red, green, and blue 8-bit components (0-255).
 */
uint32_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * Creates a color object from red, green, blue, and alpha 8-bit components (0-255).
 */
uint32_t gfx_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/**
 * Creates a color object from red, green, and blue floating point components (0.0 - 1.0).
 */
uint32_t gfx_rgbf(float r, float g, float b);

/**
 * Creates a color object from red, green, blue, and alpha floating point components (0.0 - 1.0).
 */
uint32_t gfx_rgbaf(float r, float g, float b, float a);

uint32_t gfx_color_mult(uint32_t c, float m);

/**
 * Loads one texture from a PNG file, and adds it to a shared atlas.
 *
 * @param   path    the absolute path to the PNG file.
 * @return  a valid texture index, or -1 if texture load failed.
 */
int32_t gfx_load_tex(const char *path);

/**
 * Loads one texture atlas from a PNG file.
 *
 * @param   path    the absolute path to the PNG file.
 * @param   col     the number of columns in the atlas.
 * @param   row     the number of rows in the atlas.
 * @param   w       a pointer to an integer, will be filled with the width of each atlas tile.
 * @param   h       a pointer to an integer, will be filled with the height of each atlas tile.
 * @return  the texture index of the first atlas tile, or -1 if texture load failed.
 */
int32_t gfx_load_tex_atlas(const char *path, int col, int row, int *w, int *h);

typedef enum {
    GFX_ALIGN_LEFT,
    GFX_ALIGN_CENTER,
    GFX_ALIGN_RIGHT,
} gfx_text_align_t;

typedef struct {
    char        first_char;
    int32_t     char_count;
    bool        short_dot;
} gfx_bitmap_font_desc_t;

/**
 * Loads a bitmap font from an atlas file.
 *
 * The atlas is assumed to contain 16 columns by 4 rows.
 *
 * @param   path    the absolute path to the PNG file.
 * @param   desc    the loader description for the font.
 * @return  the font index, used when drawing text. 
 */
int32_t gfx_load_bitmap_font(const char *path, const gfx_bitmap_font_desc_t *desc);

typedef struct {
    float x_bearing;
    float y_bearing;
    float width;
    float height;
    float x_advance;
    float y_advance;
} gfx_text_extents_t;

/*
 * Loads a TrueType font face from a file.
 *
 * @param   path        the absolute path to the TTF file.
 * @param   all_utf8    whether the full UTF-8 charset should be loaded, or only ASCII.
 * @return  the font index, used when drawing text.
 */
int32_t gfx_load_ttf_font(const char *path, bool all_utf8);

/**
 * Creates a new GFX context.
 * 
 * Contexts keep track of the transform stack, draw colour, line width,
 * and active bitmap and TTF fonts.
 *
 * @return  A pointer to the new context.
 */
gfx_ctx_t *gfx_new();

/**
 * Destroys a GFX context.
 *
 * @param   ctx     A pointer to the context to destroy.
 */
void gfx_destroy(gfx_ctx_t *ctx);

/**
 * Starts a new rendering frame.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_begin_frame(gfx_ctx_t *ctx);

/**
 * Ends the rendering frame.
 *
 * No work is done, but this allows the context to check that the rendering calls were
 * well-formed, and that context save & restores are well balanced.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_end_frame(gfx_ctx_t *ctx);

/***************************************************************************************************
 *
 * Vector Drawing
 *
 * Vector drawing using GFX is done using paths. You first construct one or multiple paths
 * using the various functions of the path API; then call `gfx_stroke()`, `gfx_clip()`, or
 * `gfx_fill()` to paint the path onto the context.
 *
 * A path can contain multiple _sub paths_: every time `gfx_move_to()` is called, a new
 * sub-path is created. When calling `gfx_stroke()`, distinct sub-paths are drawn
 * disconnected.
 *
 * Calls to `gfx_set_color()`, `gfx_set_line_width()`, take effect when `gfx_stroke() or
 * `gfx_fill()` is called.
 *
 **************************************************************************************************/

/**
 * Sets the color used to draw in a context.
 *
 * @param   ctx     Pointer to the context.
 * @param   color   The color in which lines, polygons and text should be drawn.
 */
void gfx_set_color(gfx_ctx_t *ctx, uint32_t color);

/**
 * Sets the width at which lines should be drawn in a context.
 *
 * @param   ctx     Pointer to the context.
 * @param   width   The width at which lines should be drawn.
 */
void gfx_set_line_width(gfx_ctx_t *ctx, float width);

typedef enum gfx_line_cap_t {
    GFX_LINE_CAP_BUTT = 0,
    GFX_LINE_CAP_ROUND = 1,
    GFX_LINE_CAP_SQUARE = 2,
} gfx_line_cap_t;

/**
 * Sets the caps with which lines should be drawn in a context.
 *
 * @param   ctx     Pointer to the context.
 * @param   cap     The cap with which lines should be drawn.
 */
void gfx_set_line_cap(gfx_ctx_t *ctx, gfx_line_cap_t cap);

/**
 * Draws a context's current path as a line, then discards the path.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_stroke(gfx_ctx_t *ctx);

/**
 * Draws a context's current path as a line, and keep the path.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_stroke_preserve(gfx_ctx_t *ctx);

/**
 * Draws a context's current path as a line, then discards the path.
 *
 * Unlike `gfx_stroke(gfx_ctx_t *)`, this does check whether the path is closed or not,
 * and just submits the vertices as-is to `XPLMLinesWithWidth()`.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_stroke_fast(gfx_ctx_t *ctx);

/**
 * Fills a context's current path as a convex polygon, and discards the path.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_fill(gfx_ctx_t *ctx);

/**
 * Fills a context's current path as a convex polygon, and keep the path.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_fill_preserve(gfx_ctx_t *ctx);

/**
 * Uses a context's current path as a convex polygon to create a clipping area,
 * and discard the path.
 *
 * Any drawing submitted after `gfx_clip()` and before `gfx_clip_reset()` is only shown when
 * it lies within the polygon's area.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_clip(gfx_ctx_t *ctx);

/**
 * Uses a context's current path as a convex polygon to create a clipping area,
 * and keep the path.
 *
 * Any drawing submitted after `gfx_clip()` and before `gfx_clip_reset()` is only shown when
 * it lies within the polygon's area.
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_clip_preserve(gfx_ctx_t *ctx);

/**
 * Discard a context's current clipping area.
 *
 *
 * @param   ctx     Pointer to the context.
 */
void gfx_clip_reset(gfx_ctx_t *ctx);

/**
 * Begin a new sub-path in a context. If the current path already has point, there will
 * be a gap between the last point and the new co-ordinates.
 *
 * After this call, the current point will be `(x, y)`.
 *
 * @param   ctx     Pointer to the context.
 * @param   x       x co-ordinate of the new point.
 * @param   y       x co-ordinate of the new point.
 */
void gfx_move_to(gfx_ctx_t *ctx, float x, float y);

/**
 * Adds a line to the current path, from the current position to the new co-ordinates.
 *
 * If there is no current position, calling `gfx_line_to()` is equivalent to
 * calling `gfx_move_to()`.
 *
 * After this call, the current point will be `(x, y)`.
 *
 * @param   ctx     Pointer to the context.
 * @param   x       x co-ordinate of the new point.
 * @param   y       x co-ordinate of the new point.
 */
void gfx_line_to(gfx_ctx_t *ctx, float x, float y);

/**
 * Adds a cubic bezier curve to the current path.
 *
 * The curve joins the current point to `(x3, y3)`, with `(x1, y1)` and `(x2, y2)`
 * as control points. If there is no current point, the curve begins at `(x1, y1)`.
 *
 * @param   ctx     Pointer to the context.
 * @param   x1      x co-ordinate of the first control point.
 * @param   y1      y co-ordinate of the first control point.
 * @param   x2      x co-ordinate of the second control point.
 * @param   y2      y co-ordinate of the second control point.
 * @param   x3      x co-ordinate of the curve's end point.
 * @param   y3      y co-ordinate of the curve's end point.
 */
void gfx_curve_to(gfx_ctx_t *ctx, float x1, float y1, float x2, float y2, float x3, float y3);

/**
 * Closes the current sub-path, by adding a point in the same location as the
 * sub-path's first point.
 *
 * @param   ctx         Pointer to the context.
 */
void gfx_close_path(gfx_ctx_t *ctx);

/**
 * Adds an circular arc of the given `radius` to the current path.
 *
 * The arc is centered at `(x, y)` at the centre, begins at `angle_start`,
 * and progresses in the direction of increasing angles to `angle_end`.
 *
 * Angles are in radians; the point at 0 radians lies on the x-axis, and angles
 * increase counter-clockwise.
 *
 * After this call, the path's current point is the point on the arc at `angle_end`.
 *
 * @param   ctx         Pointer to the context.
 * @param   x           x co-ordinate of the arc center.
 * @param   y           y co-ordinate of the arc center.
 * @param   angle_start Angle at which the arc begins.
 * @param   angle_end   Angle at which the arc begins.
 */
void gfx_arc(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float radius,
    float angle_start,
    float angle_end
);

/**
 * Adds an circular arc of the given `radius` to the current path.
 *
 * The arc is centered at `(x, y)` at the centre, begins at `angle_start`,
 * and progresses in the direction of decreasing angles to `angle_end`.
 *
 * @param   ctx         Pointer to the context.
 * @param   x           x co-ordinate of the arc center.
 * @param   y           y co-ordinate of the arc center.
 * @param   angle_start Angle at which the arc begins.
 * @param   angle_end   Angle at which the arc begins.
 */
void gfx_arc_negative(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float radius,
    float angle_start,
    float angle_end
);

/**
 * Adds a rectangle to the current path.
 *
 * This is a convenience function, which is exactly equivalent to calling:
 *
 *      gfx_move_to(ctx, x, y);
 *      gfx_line_to(ctx, x, y + h);
 *      gfx_line_to(ctx, x + w, y + h);
 *      gfx_line_to(ctx, x + w, y);
 *      gfx_close_path(ctx);
 *
 * @param   ctx         Pointer to the context.
 * @param   x           x co-ordinate of the lower left corner of the rectangle.
 * @param   y           y co-ordinate of the lower left corner of the rectangle.
 * @param   w           Width of the rectangle.
 */
void gfx_rectangle(gfx_ctx_t *ctx, float x, float y, float w, float h);

/**
 * Draws a rectangle at a given point, with a given texture applied to it.
 *
 * @param   ctx         Pointer to the context.
 * @param   tex         Texture applied to the rectangle.
 * @param   x           x co-ordinate of the lower left corner of the rectangle.
 * @param   y           y co-ordinate of the lower left corner of the rectangle.
 */
void gfx_tex(gfx_ctx_t *ctx, int32_t tex, float x, float y);

/***************************************************************************************************
 *
 * Transformations
 *
 * When using the path API, all co-ordinates are specified in _user space_. Drawing commands
 * submitted to X-Plane's Panel Graphics API are in _device space_: for avionics, this means
 * the x and y axes originate at the bottom left of the device screen, and increase up and
 * right; for windows, the x and y axes originate at the bottom left of the desktop space, and
 * increase up and right.
 *
 * When a new frame starts (by calling `gfx_new_frame()`), user space and device space are
 * the same. However, it can be useful to _transform_ user space, so co-ordinates are easier to
 * work with. For example, when drawing a map, it is easier to rotate the the axes by the current
 * aircraft's heading, than doing the rotation maths for each point drawn on the map.
 *
 * The transformation API is how the user space to device space relation is modified.
 *
 * The current transform state can be saved and restored, much like the OpenGL transorm stack.
 *
 **************************************************************************************************/

/**
 * Moves the user-space origin by a given `(x, y)` vector.
 *
 * @param   ctx         Pointer to the context.
 * @param   x           Translation along the x axis.
 * @param   y           Translation along the y axis.
 */
void gfx_translate(gfx_ctx_t *ctx, float x, float y);

/**
 * Rotate the user-space axes by a given angles.
 *
 * @param   ctx         Pointer to the context.
 * @param   angle       Rotation along the z axis, in radians.
 */
void gfx_rotate(gfx_ctx_t *ctx, float angle);

/**
 * Scales the user-space x- and y-axes by given factors.
 *
 * @param   ctx         Pointer to the context.
 * @param   sx          x-axis scaling factor.
 * @param   sy          y-axis scaling factor.
 */
void gfx_scale(gfx_ctx_t *ctx, float sx, float sy);

/**
 * Resets the user-space transform to the default (coincident with device space).
 *
 * @param   ctx         Pointer to the context.
 */
void gfx_reset(gfx_ctx_t *ctx);

/**
 * Saves the current user-space transform, and pushes it on the transform stack.
 *
 * @param   ctx         Pointer to the context.
 */
void gfx_save(gfx_ctx_t *ctx);

/**
 * Pops the user-space transform from the stack, and make it current.
 *
 * @param   ctx         Pointer to the context.
 */ 
void gfx_restore(gfx_ctx_t *ctx);


/***************************************************************************************************
 *
 * Text Drawing
 *
 **************************************************************************************************/

/**
 * Sets the font used for bitmap text drawing.
 *
 * @param   ctx         Pointer to the context.
 * @param   font_id     A valid bitmap font ID.
 */
void gfx_set_bitmap_font(gfx_ctx_t *ctx, int32_t font_id);

/**
 * Sets the TTF font face used for TTF text drawing.
 *
 * @param   ctx         Pointer to the context.
 * @param   font_id     A valid TTF font ID.
 */
void gfx_set_font_face(gfx_ctx_t *ctx, int32_t font_id);

/**
 * Sets the character size used for TTF text drawing.
 *
 * @param   ctx         Pointer to the context.
 * @param   size        Character size.
 */
void gfx_set_font_size(gfx_ctx_t *ctx, float size);


void gfx_draw_text(
    gfx_ctx_t *ctx,
    float x,
    float y,
    gfx_text_align_t align,
    const char *fmt,
    ...
);

void gfx_draw_text_bitmap(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float align,
    const char *fmt,
    ...
);

void gfx_draw_text_bitmap_inv(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float align,
    const char *fmt,
    ...
);

void gfx_draw_text_bitmap_ex(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float align,
    float extra_pad,
    const char *fmt,
    ...
);

void gfx_draw_text_bitmap_v(
    gfx_ctx_t *ctx,
    float x,
    float y,
    float align,
    const char *fmt,
    ...
);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ifdef _LAMINAR_GFX_H_ */
