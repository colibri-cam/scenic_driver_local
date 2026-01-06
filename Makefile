MIX = mix
PREFIX = $(MIX_APP_PATH)/priv
DEFAULT_TARGETS ?= $(PREFIX) $(PREFIX)/scenic_driver_local

$(info SCENIC_LOCAL_TARGET: $(SCENIC_LOCAL_TARGET))
ifdef SCENIC_LOCAL_GL
$(info SCENIC_LOCAL_GL: $(SCENIC_LOCAL_GL))
endif

SKIA_CFLAGS ?= $(shell pkg-config --cflags skia 2>/dev/null)
SKIA_LDFLAGS ?= $(shell pkg-config --libs skia 2>/dev/null)

SKIA_VERSION ?= 2.88.6
SKIA_VENDOR_DIR ?= $(abspath _build/skia)
SKIA_HEADER_NAMES = sk_bitmap.h sk_blender.h sk_canvas.h sk_codec.h sk_colorfilter.h sk_colorspace.h sk_data.h sk_document.h \
        sk_drawable.h sk_font.h sk_general.h sk_graphics.h sk_image.h sk_imagefilter.h sk_linker.h sk_maskfilter.h sk_matrix.h \
        sk_paint.h sk_path.h sk_patheffect.h sk_picture.h sk_pixmap.h sk_region.h sk_rrect.h sk_runtimeeffect.h sk_shader.h \
        sk_stream.h sk_string.h sk_surface.h sk_svg.h sk_textblob.h sk_typeface.h sk_types.h sk_vertices.h skottie_animation.h \
        skresources_resource_provider.h sksg_invalidation_controller.h
SKIA_HEADERS_DIR ?= $(SKIA_VENDOR_DIR)/include/c
SKIA_HEADER_FILES = $(addprefix $(SKIA_HEADERS_DIR)/,$(SKIA_HEADER_NAMES))
SKIA_NATIVE_NUGET ?= $(SKIA_VENDOR_DIR)/SkiaSharp.NativeAssets.Linux.NoDependencies.$(SKIA_VERSION).nupkg
SKIA_NATIVE_DIR ?= $(SKIA_VENDOR_DIR)/SkiaSharp.NativeAssets.Linux.NoDependencies.$(SKIA_VERSION)
SKIA_LIB_DIR ?= $(SKIA_NATIVE_DIR)/runtimes/linux-x64/native
SKIA_SHARED_LIB ?= $(SKIA_LIB_DIR)/libSkiaSharp.so

ifeq ($(strip $(SKIA_CFLAGS)),)
SKIA_CFLAGS := -I$(SKIA_HEADERS_DIR) -I$(SKIA_VENDOR_DIR)
endif

ifeq ($(strip $(SKIA_LDFLAGS)),)
SKIA_LDFLAGS := -L$(SKIA_LIB_DIR) -lSkiaSharp -Wl,-rpath,$(SKIA_LIB_DIR)
endif

SKIA_BOOTSTRAP :=

$(SKIA_NATIVE_NUGET):
	mkdir -p $(SKIA_VENDOR_DIR)
	curl -L https://www.nuget.org/api/v2/package/SkiaSharp.NativeAssets.Linux.NoDependencies/$(SKIA_VERSION) -o $@

$(SKIA_NATIVE_DIR): $(SKIA_NATIVE_NUGET)
	mkdir -p $(SKIA_NATIVE_DIR)
	unzip -qo $(SKIA_NATIVE_NUGET) -d $(SKIA_NATIVE_DIR)

$(SKIA_SHARED_LIB): $(SKIA_NATIVE_DIR)
	@test -f $@ || (echo "Skia shared library not found in downloaded package" && exit 1)

$(SKIA_HEADERS_DIR):
	mkdir -p $(SKIA_HEADERS_DIR)

$(SKIA_HEADERS_DIR)/%.h: | $(SKIA_HEADERS_DIR)
	curl -L https://raw.githubusercontent.com/mono/skia/master/include/c/$(@F) -o $@

DEVICE_SRCS =

FONT_SRCS = \
	c_src/font/font.c

IMAGE_SRCS = \
	c_src/image/image.c

TOMMYDS_SRCS = \
	c_src/tommyds/src/tommyhashlin.c \
	c_src/tommyds/src/tommyhash.c

SCENIC_SRCS = \
	c_src/scenic/comms.c \
	c_src/scenic/scenic_ops.c \
	c_src/scenic/script_ops.c \
	c_src/scenic/script.c \
	c_src/scenic/unix_comms.c \
	c_src/scenic/utils.c

NVG_COMMON_SRCS = \
	c_src/device/nvg/gl_helpers.c \
	c_src/device/nvg/nanovg/nanovg.c \
	c_src/device/nvg/nvg_font_ops.c \
	c_src/device/nvg/nvg_image_ops.c \
	c_src/device/nvg/nvg_scenic.c \
	c_src/device/nvg/nvg_script_ops.c

CAIRO_COMMON_SRCS = \
        c_src/device/cairo/cairo_common.c \
        c_src/device/cairo/cairo_font_ops.c \
        c_src/device/cairo/cairo_image_ops.c \
        c_src/device/cairo/cairo_script_ops.c

SKIA_COMMON_SRCS = \
        c_src/device/skia/skia_common.c \
        c_src/device/skia/skia_font_ops.c \
        c_src/device/skia/skia_image_ops.c \
        c_src/device/skia/skia_script_ops.c

ifeq ($(SCENIC_LOCAL_TARGET),cairo-gtk)
	CFLAGS = -O3 -std=gnu99

	ifndef MIX_ENV
		MIX_ENV = dev
	endif

	ifdef DEBUG
		CFLAGS += -O0 -pedantic -Wall -Wextra -Wno-unused-parameter
	endif

	ifeq ($(MIX_ENV),dev)
		CFLAGS += -g
	endif

	LDFLAGS += `pkg-config --static --libs freetype2 cairo gtk+-3.0`
	CFLAGS += `pkg-config --static --cflags freetype2 cairo gtk+-3.0`
	LDFLAGS += -lm

	DEVICE_SRCS += \
		$(CAIRO_COMMON_SRCS) \
		c_src/device/cairo/cairo_gtk.c

else ifeq ($(SCENIC_LOCAL_TARGET),cairo-fb)
        LDFLAGS += `pkg-config --static --libs freetype2 cairo`
        CFLAGS += `pkg-config --static --cflags freetype2 cairo`
        LDFLAGS += -lm
        CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -pedantic
        CFLAGS += -std=gnu99

        DEVICE_SRCS += \
                $(CAIRO_COMMON_SRCS) \
                c_src/device/cairo/cairo_fb.c

else ifeq ($(SCENIC_LOCAL_TARGET),skia-fb)
        CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -pedantic
        CFLAGS += -std=gnu99

        SKIA_BOOTSTRAP += $(SKIA_SHARED_LIB) $(SKIA_HEADER_FILES)

        CFLAGS += $(SKIA_CFLAGS)
        LDFLAGS += $(SKIA_LDFLAGS) -lm

        DEVICE_SRCS += \
                $(SKIA_COMMON_SRCS) \
                c_src/device/skia/skia_fb.c

else ifeq ($(SCENIC_LOCAL_TARGET),skia-glfw)
        CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -pedantic
        CFLAGS += -std=gnu99

        SKIA_BOOTSTRAP += $(SKIA_SHARED_LIB) $(SKIA_HEADER_FILES)

        CFLAGS += $(SKIA_CFLAGS) `pkg-config --static --cflags glfw3 glew`
        LDFLAGS += $(SKIA_LDFLAGS) `pkg-config --static --libs glfw3 glew` -lm

        DEVICE_SRCS += \
                $(SKIA_COMMON_SRCS) \
                c_src/device/skia/skia_glfw.c

else ifeq ($(SCENIC_LOCAL_TARGET),glfw)
$(info )
$(info **********************************************************************************)
$(info SCENIC_LOCAL_TARGET=glfw is deprecated. Please use `SCENIC_LOCAL_TARGET=cairo-gtk`)
$(info **********************************************************************************)
$(info )

	CFLAGS = -O3 -std=c99

	ifndef MIX_ENV
		MIX_ENV = dev
	endif

	ifdef DEBUG
		CFLAGS +=  -pedantic -Weverything -Wall -Wextra -Wno-unused-parameter -Wno-gnu
	endif

	ifeq ($(MIX_ENV),dev)
		CFLAGS += -g
	endif

	LDFLAGS += `pkg-config --static --libs glfw3 glew`
	CFLAGS += `pkg-config --static --cflags glfw3 glew`

	ifneq ($(OS),Windows_NT)
		CFLAGS += -fPIC

		ifeq ($(shell uname),Darwin)
			LDFLAGS += -framework Cocoa -framework OpenGL -Wno-deprecated
		else
			LDFLAGS += -lGL -lm -lrt
		endif
	endif

	DEVICE_SRCS += \
		$(NVG_COMMON_SRCS) \
		c_src/device/nvg/glfw.c

else ifeq ($(SCENIC_LOCAL_TARGET),bcm)
$(info )
$(info ********************************************************************************)
$(info SCENIC_LOCAL_TARGET=bcm is deprecated. Please use `SCENIC_LOCAL_TARGET=cairo-fb`)
$(info ********************************************************************************)
$(info )

	LDFLAGS += -lGLESv2 -lEGL -lm -lvchostif -lbcm_host
	CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -pedantic
	CFLAGS += -std=gnu99

	DEVICE_SRCS += \
		$(NVG_COMMON_SRCS) \
		c_src/device/nvg/bcm.c

	ifeq ($(SCENIC_LOCAL_GL),gles2)
		CFLAGS += -DSCENIC_GLES2
	else
		CFLAGS += -DSCENIC_GLES3
	endif

else ifeq ($(SCENIC_LOCAL_TARGET),drm)
$(info )
$(info ********************************************************************************)
$(info SCENIC_LOCAL_TARGET=drm is deprecated. Please use `SCENIC_LOCAL_TARGET=cairo-fb`)
$(info ********************************************************************************)
$(info )

	LDFLAGS += -lGLESv2 -lEGL -lm -lvchostif -ldrm -lgbm
	CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -pedantic
	CFLAGS += -std=gnu99
	CFLAGS += -fPIC -I$(NERVES_SDK_SYSROOT)/usr/include/drm

	DEVICE_SRCS += \
		$(NVG_COMMON_SRCS) \
		c_src/device/nvg/drm.c

	ifeq ($(SCENIC_LOCAL_GL),gles2)
		CFLAGS += -DSCENIC_GLES2
	else
		CFLAGS += -DSCENIC_GLES3
	endif

endif

CFLAGS += \
	-Ic_src \
	-Ic_src/device \
	-Ic_src/font \
	-Ic_src/image \
	-Ic_src/scenic \
	-Ic_src/tommyds/src

SRCS = \
	$(DEVICE_SRCS) \
	$(FONT_SRCS) \
	$(IMAGE_SRCS) \
	$(TOMMYDS_SRCS) \
	$(SCENIC_SRCS) \
	c_src/main.c

calling_from_make:
	mix compile

all: $(DEFAULT_TARGETS)

$(PREFIX):
	mkdir -p $@

$(PREFIX)/scenic_driver_local: $(SRCS) $(SKIA_BOOTSTRAP)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	$(RM) -rf $(PREFIX)

.PHONY: all clean calling_from_make

