#pragma once

#include "ra_d3d11.h"

struct d3d11_upscaler;

struct d3d11_upscaler *d3d11_upscaler_create(struct ra_ctx * ctx, ID3D11Device *device);

static void do_nvidia_rtx_super_resolution(struct ra_ctx *ctx, struct d3d11_upscaler * upscaler);

bool d3d11_upscaler_update(struct ra_ctx *ctx, ID3D11Device *device, struct d3d11_upscaler *upscaler, int output_width, int output_height);

bool d3d11_upscaler_upscale(struct ra_ctx *ctx, struct d3d11_upscaler *upscaler, ID3D11Texture2D *backbuffer);
