
#include <d3d11.h>
#include <d3d11_1.h>

#include "d3d11_upscaler.h"

struct d3d11_upscaler {
	UINT                            width;
    UINT                            height;
	char 							say_hellow[10];
	
	ID3D11Device		 			*device;
	ID3D11DeviceContext 			*device_context;
	
	ID3D11VideoDevice               *video_device;
    ID3D11VideoContext              *video_context;
	
    ID3D11VideoProcessorEnumerator  *vp_enumerator;
    ID3D11VideoProcessor            *video_processor;
    
	ID3D11Texture2D 				*texture;
	
	ID3D11VideoProcessorOutputView 	*out_view;
	ID3D11VideoProcessorInputView	*in_view;
};

/*struct priv {
    ID3D11Device 					*device;
};*/

static void do_nvidia_rtx_super_resolution(struct ra_ctx *ctx, struct d3d11_upscaler * upscaler)
{

	MP_WARN(ctx, "yooo\n");
	HRESULT hr;
	
	const GUID nvGUID = {
		0xD43CE1B3,
		0x1F4B,
		0x48AC,
		{ 0xBA, 0xEE, 0xC3, 0xC2, 0x53, 0x75, 0xE6, 0xF7 }
	};
	bool enableNV = true;
	
	const UINT kStreamExtensionVersionV1 = 0x1;
	const UINT kStreamExtensionMethodRTXVSR = 0x2;
	
	struct {
	UINT version;
	UINT method;
	UINT enable;
	} streamExtensionInfo = {kStreamExtensionVersionV1, kStreamExtensionMethodRTXVSR,
						   enableNV ? 1u : 0};   

	hr = ID3D11VideoContext_VideoProcessorSetStreamExtension(
	  upscaler->video_context, upscaler->video_processor, 0, &nvGUID, sizeof(streamExtensionInfo),
	  &streamExtensionInfo); 

	if (FAILED(hr)) {
		MP_ERR(ctx, "Failed activating super res.\n");		
	}	
}

struct d3d11_upscaler *d3d11_upscaler_create(struct ra_ctx * ctx, ID3D11Device *device)
{

	HRESULT hr;
	
	ID3D11Device_AddRef(device);
	
	MP_WARN(ctx, "Creating upscaler...\n");
	
	struct d3d11_upscaler * upscaler = talloc_zero(NULL, struct d3d11_upscaler);
	
	if(!upscaler->video_context) MP_WARN(ctx, "There be no video contex. because it was just created.\n");
	
	MP_WARN(ctx, "Attempt to give upscaler a device context.\n");	
	
	ID3D11Device_GetImmediateContext(device, &upscaler->device_context);	

	MP_WARN(ctx, "Getting upscaler video device.\n");
	
	hr = ID3D11Device_QueryInterface(device, &IID_ID3D11VideoDevice,
                                     (void **)&upscaler->video_device);
	if (FAILED(hr)) {
		MP_ERR(ctx, "Eror getting upscaler video device.\n");		
	}
	
	MP_WARN(ctx, "Getting upscaler device context.\n");
	
	hr = ID3D11DeviceContext_QueryInterface(upscaler->device_context, &IID_ID3D11VideoContext,
                                     (void **)&upscaler->video_context);
	if (FAILED(hr)) {
		MP_ERR(ctx, "Eror getting upscaler device context.\n");		
	}
	
	MP_WARN(ctx, "Got the upscaler video device.\n");
	
	strcpy(upscaler->say_hellow, "hello you!");
	upscaler->width = 1;
	upscaler->height = 1;
	upscaler->out_view = NULL;
	
	ID3D11Device_Release(device);
	
	return upscaler;

fail:
	talloc_free(upscaler);
	ID3D11Device_Release(device);
	return NULL;
}

bool d3d11_upscaler_update(struct ra_ctx *ctx, ID3D11Device *device, struct d3d11_upscaler *upscaler, int output_width, int output_height)
{
	MP_WARN(ctx, "Attempting to update scaler.\n");
	HRESULT hr;
	ID3D11Device_AddRef(device);
	
	
	if (!upscaler->vp_enumerator)
    {
		MP_WARN(ctx, "No upscaler enumerator found. Creating now...\n");
		D3D11_VIDEO_FRAME_FORMAT d3d_frame_format;
		d3d_frame_format = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
		
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpdesc = {
			.InputFrameFormat = d3d_frame_format,
			.InputWidth = 854,
			.InputHeight = 480,
			.OutputWidth = output_width,
			.OutputHeight = output_height,
		};
		
		hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(upscaler->video_device, &vpdesc,
															  &upscaler->vp_enumerator);
        if (FAILED(hr))
        {
            MP_ERR(ctx, "upscaler processor enum go brrrrr.\n");
            goto fail;
        }
		
		MP_WARN(ctx, "Enumerator done. Creating upscaler video processor...\n");

        hr = ID3D11VideoDevice_CreateVideoProcessor(upscaler->video_device,  upscaler->vp_enumerator, 0,
                                                        &upscaler->video_processor);
        if (FAILED(hr))
        {
            MP_ERR(ctx, "no upscaler video processor for you.\n");
            goto fail;
        }
    }
	
	if(upscaler->width != output_width || upscaler->height != output_height)
	{
		MP_ERR(ctx, "Attempting to create texture.\n");
		upscaler->width = output_width;
		upscaler->height = output_height;	
		
		D3D11_TEXTURE2D_DESC texDesc = {
			.MipLevels = 1,
			.SampleDesc.Count = 1,
			.MiscFlags = 0,
			.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			.Usage = D3D11_USAGE_DEFAULT,
			.CPUAccessFlags = 0,
			.ArraySize = 1,
			.Width = upscaler->width % 2 == 0 ? upscaler->width : upscaler->width - 1 ,
			.Height = upscaler->height % 2 == 0 ? upscaler->height: upscaler->height - 1,
			.Format = DXGI_FORMAT_NV12,
		};
		
		MP_ERR(ctx, "Creating texture.\n");
		
		hr = ID3D11Device_CreateTexture2D(device, &texDesc, NULL, &upscaler->texture);
		if (FAILED(hr))
		{
			MP_ERR(ctx, "upscale texture failed.\n");
			goto fail;
		}
		
		D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outdesc = {
			.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
		};
		
		if (upscaler->out_view)
			ID3D11VideoProcessorOutputView_Release(upscaler->out_view);
		
		hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(upscaler->video_device,
															  (ID3D11Resource *)upscaler->texture,
															  upscaler->vp_enumerator, &outdesc,
															  &upscaler->out_view);
		if (FAILED(hr))
		{
			MP_ERR(ctx, "out view die.\n");
			goto fail;
		}
	}
	
	RECT src_rc = {
		.right = 854,
		.bottom = 480,
	};
	
    RECT dest_rc = {   
		.right  = output_width,
		.bottom = output_height,
	};
	
	MP_WARN(ctx, "Creating stream source rect.\n");
	
	if(!upscaler->video_processor) MP_WARN(ctx, "there be no video context. but there should be...\n");

    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(upscaler->video_context,
															upscaler->video_processor,
                                                            0, TRUE, &src_rc);

	MP_WARN(ctx, "Creating stream dest rect.\n");

    ID3D11VideoContext_VideoProcessorSetStreamDestRect(upscaler->video_context,
														upscaler->video_processor,
                                                          0, TRUE, &dest_rc);
														  
	MP_WARN(ctx, "Activating super res.\n");

	do_nvidia_rtx_super_resolution(ctx, upscaler);
	
	MP_WARN(ctx, "Activated super res\n.");	
	
	ID3D11Device_Release(device);
	return true;
	
fail:
	ID3D11Device_Release(device);
	if (upscaler->vp_enumerator)
        ID3D11VideoProcessorEnumerator_Release(upscaler->vp_enumerator);
	if (upscaler->vp_enumerator)
        ID3D11VideoProcessorEnumerator_Release(upscaler->vp_enumerator);
	if (upscaler->video_processor)
        ID3D11VideoProcessorOutputView_Release(upscaler->video_processor);
	return false;
}


bool d3d11_upscaler_upscale(struct ra_ctx *ctx, struct d3d11_upscaler *upscaler, ID3D11Texture2D *backbuffer)
{
	ID3D11Texture2D_AddRef(backbuffer);
	HRESULT hr;
	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC indesc = {
        .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
    };
	
	MP_WARN(ctx, "Attempting to create input view.\n");
	
	hr = ID3D11VideoDevice_CreateVideoProcessorInputView(
										 upscaler->video_device,
										 (ID3D11Resource *)backbuffer,
										 upscaler->vp_enumerator,
										 &indesc,
										 &upscaler->in_view);
	if (FAILED(hr))
	{
		MP_ERR(ctx, "There was an eror creating input view.\n");
		goto fail;
    }
	
	MP_WARN(ctx, "Attempting to blt.\n");
	
	D3D11_VIDEO_PROCESSOR_STREAM stream = {
        .Enable = TRUE,
        .pInputSurface = upscaler->in_view,
    };
	
    hr = ID3D11VideoContext_VideoProcessorBlt(upscaler->video_context, 
												upscaler->video_processor,
												upscaler->out_view, 0, 1, &stream);
	if (FAILED(hr))
	{
		MP_ERR(ctx, "There was an eror while blting.\n");
		goto fail;
    }
	
	ID3D11Texture2D_Release(backbuffer);
    return true;
	
fail:
	ID3D11Texture2D_Release(backbuffer);
	if (upscaler->in_view)
        ID3D11VideoProcessorEnumerator_Release(upscaler->in_view);
	return false;
}

