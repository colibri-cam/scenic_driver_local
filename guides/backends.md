## Backend structure overview

The driver chooses its rendering backend at compile time through the
`SCENIC_LOCAL_TARGET` make variable. Existing options are `cairo-gtk`,
`cairo-fb`, `skia-fb`, `skia-glfw`, `glfw`, `bcm`, and `drm` (all configured in
the root `Makefile`).
Each target supplies a backend-specific set of C sources and the required
compiler/linker flags (for example, Cairo and GTK `pkg-config` entries for
`cairo-gtk`, or NanoVG plus GLES for the GL targets).

Regardless of the selected backend, the driver uses the functions declared in
`c_src/device/device.h`:

* `int device_init(const device_opts_t*, device_info_t*, driver_data_t*);`
* `int device_close(device_info_t*);`
* `void device_poll();`
* `void device_loop(driver_data_t*);`
* `void device_begin_render(driver_data_t*);`
* `void device_begin_cursor_render(driver_data_t*);`
* `void device_end_render(driver_data_t*);`
* `void device_clear_color(float red, float green, float blue, float alpha);`
* `char* device_gl_error();`

Backend implementations supply these functions and place any renderer-specific
state into `device_info_t.v_ctx` for later callbacks during the scenic render
loop (`render()` in `c_src/scenic/comms.c`). The lifecycle is:

1. `device_init` creates the rendering context, wires it into `device_info_t`,
   and prepares the output surface (window, framebuffer, etc.).
2. `device_begin_render` resets per-frame state and clears the render target to
   the color set via `device_clear_color`.
3. Scenic script execution calls `script_ops_*` functions (listed below) to
   draw into the backend context.
4. `device_begin_cursor_render` applies the cursor transform before rendering
   the optional cursor scene.
5. `device_end_render` presents the finished frame (for framebuffers this means
   copying the raster surface into the device memory; for GL it swaps buffers).
6. `device_close` frees backend resources and cleans up device state.

### Desktop Skia (GLFW/EGL)

The `skia-glfw` target provides a GPU-backed desktop renderer. It uses GLFW to
open a Wayland- or X11-hosted OpenGL context (preferring EGL for Wayland),
creates a Skia `gr_direct_context_t` from the native GL interface, and wraps the
default framebuffer in a `gr_backendrendertarget_t`/`sk_surface_t` pair. Window
resizing rebuilds the backend render target and propagates shape/input events
through the existing GLFW keymap. Build requirements mirror the runtime needs:
Skia with GPU support plus `glfw3` and `glew` development headers and libraries.

### Script, font, and image hooks

Backends also implement the script/font/image hooks declared in
`c_src/scenic/script_ops.h`, `c_src/font/font_ops.h`, and
`c_src/image/image_ops.h`. The Cairo and NanoVG backends override every
`script_ops_*` symbol to map Scenic drawing opcodes to their rendering APIs and
rely on the weak defaults in `c_src/scenic/script_ops.c` when no backend
implementation is linked. The key entry points a backend must provide are:

* Shape drawing and path control: `script_ops_draw_*`, `script_ops_begin_path`,
  `script_ops_close_path`, `script_ops_fill_path`, `script_ops_stroke_path`,
  `script_ops_move_to`, `script_ops_line_to`, `script_ops_arc_to`,
  `script_ops_bezier_to`, `script_ops_quadratic_to`, `script_ops_arc`.
* State management: `script_ops_push_state`, `script_ops_pop_state`,
  `script_ops_scissor`, and transform helpers (`transform`, `scale`, `rotate`,
  `translate`).
* Paint configuration: fill/stroke colors and gradients, image/stream fills,
  stroke width, caps/joins/miter limits, and text settings (font, size,
  alignment, baseline).
* Font/image resources: `font_ops_create` creates font handles from in-memory
  blobs, while `image_ops_create`, `image_ops_update`, and `image_ops_delete`
  manage image handles usable by `script_ops_fill_image`/`stroke_image`.

The Cairo backend keeps renderer state in `scenic_cairo_ctx_t` (see
`c_src/device/cairo/cairo_ctx.h`) while NanoVG uses its GL context directly.
Each backend must track its surface, active paints/patterns, font and image
registries, and any output transport (for example, `c_src/device/cairo/cairo_fb.c`
copies the Cairo image surface into `/dev/fb0`). The Skia backend should mirror
this layout so it can be slotted into the same lifecycle and function table
without altering the Scenic rendering flow.
