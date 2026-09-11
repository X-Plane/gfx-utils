GFX-utils
=========

Canvas-like API for X-Plane 12.4.4+ PanelGraphics drawing.

GFX abstracts away XPLMPanelGraphics' vertices, and instead provides you with a stateful
canvas-like API to draw lines, arcs, Bézier curves, TTF- and bitmap-font text.

Note that GFX doesn't cover the whole XPLMPanelGraphics API surface. Pull Requests are welcome!

Quick Start Guide
-----------------

To draw anything, you must first:

- initialise GFX's shared data;
- bake the shared data, once your resources (texture atlases and bitmap fonts) have been loaded;
- create a context for each device or window you're drawing in.

Then, in your device or window draw callback:

- call `gfx_begin_frame(gfx_ctx_t *)`;
- draw anything;
- call `gfx_end_frame(gfx_ctx_t *)`.

```c
#include <gfx.h>

static gfx_ctx_t    *device_ctx;
static int32_t      some_texture;

int XPluginStart() {
    gfx_init_shared();
    some_texture = gfx_load_tex("resources/texture.png");
    gfx_bake_shared();
    
    device_ctx = gfx_new();
}

void XPluginStop() {
    gfx_destroy(device_ctx);
    gfx_fini_shared();
}

void device_draw_callback() {
    gfx_begin_frame(device_ctx);
    
    // Now draw something!
    gfx_set_color(device_ctx, gfx_rgbf(1.f, 0.f, 1.f));
    gfx_arc(device_ctx, 100, 100, 50, 0, 2 * M_PI);
    gfx_fill(device_ctx);
    
    gfx_end_frame(device_ctx);
}
```

API Reference
-------------

The API is documented in the header. If you find something that doesn't match the documentation,
please let me know by emailing <amy@x-plane.com>.
