//----------------------------------------------------------------------------------------------
// Main.cpp
//----------------------------------------------------------------------------------------------
#include "StdAfx.h"
#include "WICTextureLoader.h"

#define INPUT_IMAGE L"..\\Media\\background.bmp"
//#define INPUT_IMAGE L"..\\Media\\7206.bmp"
//#define INPUT_IMAGE L"..\\Media\\even_height_1280x858.bmp"
//#define INPUT_IMAGE L"..\\Media\\even_height_1278x860.bmp"
//#define INPUT_IMAGE L"..\\Media\\even_height.bmp"
#define CONVERTED1_IMAGE L"ConvertedImage1.bmp"
#define CONVERTED2_IMAGE L"ConvertedImage2.bmp"
#define CONVERTED3_IMAGE L"ConvertedImage3.bmp"

void main()
{
	if(FAILED(CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE)))
		return;

	CD3D11ShaderNV12 cD3D11ShaderNV12;

	if (FAILED(cD3D11ShaderNV12.InitD3D11()))
	{
		cD3D11ShaderNV12.OnRelease();
		CoUninitialize();
		return;
	}

	if (FAILED(DirectX::CreateWICTextureFromFileEx(cD3D11ShaderNV12.GetD3D11Device(), INPUT_IMAGE, UINT32_MAX,
		D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, DirectX::WIC_LOADER_FORCE_RGBA32, NULL, &cD3D11ShaderNV12.GetInputSRV())))
	{
		cD3D11ShaderNV12.OnRelease();
		CoUninitialize();
		return;
	}

	if(cD3D11ShaderNV12.InitShaderNV12() == S_OK)
	{
		// CONVERT_INPUT_SHADER (OK)
		// CONVERT_SMALL_INPUT_SHADER (OK)
		// CONVERT_LUMA_SHADER (OK)
		// CONVERT_CHROMA_SHADER (OK)
		// CONVERT_LUMACHROMA_SHADER (OK)
		// CONVERT_LUMACHROMACBCR_SHADER (OK)
		// CONVERT_CHROMADOWNSAMPLED_SHADER (OK)
		// CONVERT_CHROMADOWNSAMPLED2_SHADER (OK)
		// CONVERT_CHROMADOWNSAMPLED_MIPS (OK)
		// CONVERT_FAKE_NV12_SHADER (OK)
		// CONVERT_FAKE_NV12_SHADER_MIPS (OK)
		// CONVERT_NV12_SHADER (OK)
		// CONVERT_I420_SHADER(OK)
		cD3D11ShaderNV12.ProcessShaderNV12(CONVERTED1_IMAGE, CONVERTED2_IMAGE, CONVERTED3_IMAGE, CD3D11ShaderNV12::SHADER_CONVERSION::CONVERT_I420_SHADER);
	}

	cD3D11ShaderNV12.OnRelease();
	CoUninitialize();
}