//----------------------------------------------------------------------------------------------
// D3D11ShaderNV12.cpp
//----------------------------------------------------------------------------------------------
#include "StdAfx.h"
#include <stdio.h>

using namespace Microsoft::WRL;

extern void SaveToBMP(ID3D11Device *device, ID3D11Texture2D* pTexture2D, const char* bmp_filename);

HRESULT CD3D11ShaderNV12::InitD3D11()
{
	HRESULT hr = S_OK;

	IF_FAILED_RETURN(m_pSamplerLinearState != NULL ? E_UNEXPECTED : S_OK);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	UINT uiFeatureLevels = ARRAYSIZE(featureLevels);
	D3D_FEATURE_LEVEL featureLevel;
	UINT uiD3D11CreateFlag = D3D11_CREATE_DEVICE_SINGLETHREADED;
	//-->UINT uiFormatSupport;

#ifdef _DEBUG
	uiD3D11CreateFlag |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	try
	{
		IF_FAILED_THROW(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, uiD3D11CreateFlag, featureLevels, uiFeatureLevels, D3D11_SDK_VERSION, &m_pD3D11Device, &featureLevel, &m_pD3D11DeviceContext));
		//-->IF_FAILED_THROW(m_pD3D11Device->CheckFormatSupport(DXGI_FORMAT_NV12, &uiFormatSupport));
	}
	catch (HRESULT) {}

	return hr;
}

ID3D11Device* CD3D11ShaderNV12::GetD3D11Device()
{
	return m_pD3D11Device;
}

ID3D11ShaderResourceView* &CD3D11ShaderNV12::GetInputSRV()
{
	return m_pInputRSV;
}

HRESULT CD3D11ShaderNV12::InitShaderNV12()
{
	HRESULT hr = S_OK;
	ID3D11Resource* pResource = NULL;
	ID3D11Texture2D* pTexture2D = NULL;
	D3D11_RESOURCE_DIMENSION resDIM = D3D11_RESOURCE_DIMENSION_UNKNOWN;

	IF_FAILED_RETURN(m_pSamplerLinearState != NULL ? E_UNEXPECTED : S_OK);

	try
	{
		m_pInputRSV->GetResource(&pResource);
		pResource->GetType(&resDIM);
		if (resDIM == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		{
			IF_FAILED_THROW(pResource->QueryInterface(IID_PPV_ARGS(&pTexture2D)));
			pTexture2D->GetDesc(&m_input_texture2d_desc);
			IF_FAILED_THROW(InitVertexPixelShaders());
			IF_FAILED_THROW(InitTextures(m_input_texture2d_desc.Width, m_input_texture2d_desc.Height));
			IF_FAILED_THROW(InitD3D11Resources(m_input_texture2d_desc.Width, m_input_texture2d_desc.Height));

		}
		else
			hr = E_NOTIMPL;
	}
	catch(HRESULT){}

	SAFE_RELEASE(pResource);
	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::ProcessShaderNV12(LPCWSTR wszOutputImageFile1, LPCWSTR wszOutputImageFile2, LPCWSTR wszOutputImageFile3, const enum SHADER_CONVERSION ShaderConversion)
{
	HRESULT hr = S_OK;

	IF_FAILED_RETURN(m_pSamplerLinearState == NULL ? E_UNEXPECTED : S_OK);

	UINT uiWidth = m_input_texture2d_desc.Width;
	UINT uiHeight = m_input_texture2d_desc.Height;

	if (uiWidth == 0 || uiHeight == 0)
		return E_NOTIMPL;

	if(ShaderConversion == CONVERT_INPUT_SHADER)
	{
		ProcessInputShader();

		IF_FAILED_RETURN(CreateBmpFileFromRgbaSurface(m_pViewRT, wszOutputImageFile1));
	}
	else if(ShaderConversion == CONVERT_SMALL_INPUT_SHADER)
	{
		InitViewPort(uiWidth / 2, uiHeight / 2);
		m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinearState);
		ProcessSmallInputShader();

		IF_FAILED_RETURN(CreateBmpFileFromRgbaSurface(m_pSmallViewRT, wszOutputImageFile1));
	}
	else if(ShaderConversion == CONVERT_LUMA_SHADER)
	{
		ProcessLumaShader();

		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pLumaRT, wszOutputImageFile1));
	}
	else if(ShaderConversion == CONVERT_CHROMA_SHADER)
	{
		ProcessChromaShader();

		IF_FAILED_RETURN(CreateBmpFileFromChromaSurface(m_pChromaRT, wszOutputImageFile1, TRUE));
		IF_FAILED_RETURN(CreateBmpFileFromChromaSurface(m_pChromaRT, wszOutputImageFile2, FALSE));
	}
	else if(ShaderConversion == CONVERT_LUMACHROMA_SHADER)
	{
		ProcessYCbCrShader();

		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pLumaRT, wszOutputImageFile1));
		IF_FAILED_RETURN(CreateBmpFileFromChromaSurface(m_pChromaRT, wszOutputImageFile2, TRUE));
		IF_FAILED_RETURN(CreateBmpFileFromChromaSurface(m_pChromaRT, wszOutputImageFile3, FALSE));
	}
	else if(ShaderConversion == CONVERT_LUMACHROMACBCR_SHADER)
	{
		ProcessYCbCrShader2();

		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pLumaRT, wszOutputImageFile1));
		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pChromaCBRT, wszOutputImageFile2));
		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pChromaCRRT, wszOutputImageFile3));
	}
	else if(ShaderConversion == CONVERT_CHROMADOWNSAMPLED_SHADER)
	{
		ProcessYCbCrShader();

		InitViewPort(uiWidth / 2, uiHeight / 2);
		m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinearState);
		ProcessChromaDownSampledShader();

		IF_FAILED_RETURN(CreateBmpFileFromChromaSurface(m_pChromaDownSampledRT, wszOutputImageFile1, TRUE));
		IF_FAILED_RETURN(CreateBmpFileFromChromaSurface(m_pChromaDownSampledRT, wszOutputImageFile2, FALSE));
		IF_FAILED_RETURN(CreateBmpFileFromLumaChromaDownSampledSurface(m_pLumaRT, m_pChromaDownSampledRT, wszOutputImageFile3));
	}
	else if(ShaderConversion == CONVERT_CHROMADOWNSAMPLED2_SHADER)
	{
		ProcessYCbCrShader2();

		InitViewPort(uiWidth / 2, uiHeight / 2);
		m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinearState);
		ProcessChromaDownSampledShader2();

		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pLumaRT, wszOutputImageFile1));
		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pChromaCBDownSampledRT, wszOutputImageFile2));
		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pChromaCRDownSampledRT, wszOutputImageFile3));
	}
	else if(ShaderConversion == CONVERT_CHROMADOWNSAMPLED_MIPS)
	{
		ProcessYCbCrShaderMips();

		m_pD3D11DeviceContext->GenerateMips(m_pChromaCBMipsRSV);
		m_pD3D11DeviceContext->GenerateMips(m_pChromaCRMipsRSV);

		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pLumaRT, wszOutputImageFile1));
		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pChromaCBMipsRSV, wszOutputImageFile2));
		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pChromaCRMipsRSV, wszOutputImageFile3));
	}
	else if(ShaderConversion == CONVERT_FAKE_NV12_SHADER)
	{
		ProcessYCbCrShader2();

		InitViewPort(uiWidth / 2, uiHeight / 2);
		m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinearState);
		ProcessChromaDownSampledShader2();

		InitViewPort(uiWidth, uiHeight);
		m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerPointState);
		ProcessYFakeNV12Shader();

		InitViewPort(uiWidth, uiHeight * 2);
		ProcessFakeUVShader();

		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pFakeNV12RT, wszOutputImageFile1));
		IF_FAILED_RETURN(CreateBmpFileFromNV12Surface(m_pFakeNV12RT, wszOutputImageFile2));
	}
	else if(ShaderConversion == CONVERT_FAKE_NV12_SHADER_MIPS)
	{
		ProcessYCbCrShaderMips();

		m_pD3D11DeviceContext->GenerateMips(m_pChromaCBMipsRSV);
		m_pD3D11DeviceContext->GenerateMips(m_pChromaCRMipsRSV);

		ProcessYFakeNV12Shader();

		InitViewPort(uiWidth, uiHeight * 2);
		ProcessFakeUVShaderMips();

		IF_FAILED_RETURN(CreateBmpFileFromLumaSurface(m_pFakeNV12RT, wszOutputImageFile1));
		IF_FAILED_RETURN(CreateBmpFileFromNV12Surface(m_pFakeNV12RT, wszOutputImageFile2));
	}
	else if(ShaderConversion == CONVERT_NV12_SHADER)
	{
		// Separate the Luma and Chroma planes from RGB texture
		ProcessYChromaShaderForNV12();

		// DownSample Chroma plane directly to NV12 interleaved U/V plane
		InitViewPort(uiWidth / 2, uiHeight / 2);
		m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinearState);
		ProcessChromaDownSampledShaderToNV12();
		
		//SaveNV12(m_pNV12Texture, "RGB2NV12_out.yuv", uiWidth, uiHeight);

		CreateBmpFileFromNV12Texture(m_pNV12Texture, wszOutputImageFile2);
	}
	else if (ShaderConversion == CONVERT_I420_SHADER)
	{
		ProcessYChromaShaderForI420();

		InitViewPort(uiWidth / 2, uiHeight / 2);
		m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinearState);
		ProcessChromaDownSampledShader();

		InitViewPort(uiWidth / 2, uiHeight / 2);
		m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerPointState);
		ProcessChromaDownSampledShaderToI420();

		//SaveI420(m_pI420Texture, "RGB2I420_out.yuv", uiWidth, uiHeight);
	}
	else
	{
		IF_FAILED_RETURN(E_NOTIMPL);
	}

	return hr;
}

void CD3D11ShaderNV12::OnRelease()
{
	SAFE_RELEASE(m_pViewRT);
	SAFE_RELEASE(m_pSmallViewRT);
	SAFE_RELEASE(m_pLumaRT);
	SAFE_RELEASE(m_pChromaRT);
	SAFE_RELEASE(m_pChromaCBRT);
	SAFE_RELEASE(m_pChromaCRRT);
	SAFE_RELEASE(m_pChromaCBMipsRT);
	SAFE_RELEASE(m_pChromaCRMipsRT);
	SAFE_RELEASE(m_pChromaDownSampledRT);
	SAFE_RELEASE(m_pChromaCBDownSampledRT);
	SAFE_RELEASE(m_pChromaCRDownSampledRT);
	SAFE_RELEASE(m_pFakeNV12RT);
	SAFE_RELEASE(m_pNV12LumaRT);
	SAFE_RELEASE(m_pNV12ChromaRT);
	SAFE_RELEASE(m_pI420LumaRT);
	SAFE_RELEASE(m_pI420ChromaRT);

	SAFE_RELEASE(m_pNV12Texture);
	SAFE_RELEASE(m_pI420Texture);

	SAFE_RELEASE(m_pInputRSV);
	SAFE_RELEASE(m_pLumaRSV);
	SAFE_RELEASE(m_pChromaRSV);
	SAFE_RELEASE(m_pChromaCBRSV);
	SAFE_RELEASE(m_pChromaCRRSV);
	SAFE_RELEASE(m_pChromaCBMipsRSV);
	SAFE_RELEASE(m_pChromaCRMipsRSV);
	SAFE_RELEASE(m_pChromaDownSampledRSV);
	SAFE_RELEASE(m_pChromaCBDownSampledRSV);
	SAFE_RELEASE(m_pChromaCRDownSampledRSV);
	SAFE_RELEASE(m_pShiftWidthRSV);

	SAFE_RELEASE(m_pSamplerPointState);
	SAFE_RELEASE(m_pSamplerLinearState);
	SAFE_RELEASE(m_pVertexLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pCombinedUVVertexShader);

	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pPixelShader2);
	SAFE_RELEASE(m_pPixelShader3);
	SAFE_RELEASE(m_pPixelShader4);
	SAFE_RELEASE(m_pLumaShader);
	SAFE_RELEASE(m_pChromaShader);
	SAFE_RELEASE(m_pYCbCrShader);
	SAFE_RELEASE(m_pYCbCrShader2);
	SAFE_RELEASE(m_pCombinedUVPixelShader);
	SAFE_RELEASE(m_pCombinedUVMipsPixelShader);
	SAFE_RELEASE(m_pChromaCbCrPixelShader);

	if(m_pD3D11DeviceContext)
	{
		m_pD3D11DeviceContext->ClearState();
		m_pD3D11DeviceContext->Release();
		m_pD3D11DeviceContext = NULL;
	}

	if(m_pD3D11Device){

		ULONG ulCount = m_pD3D11Device->Release();
		m_pD3D11Device = NULL;
		assert(ulCount == 0);
	}
}

void CD3D11ShaderNV12::ProcessInputShader()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pViewRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pViewRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessSmallInputShader()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pSmallViewRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pSmallViewRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessLumaShader()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pLumaRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pLumaRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pLumaShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessChromaShader()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pChromaRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pChromaRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pChromaShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessYCbCrShader()
{
	ID3D11RenderTargetView* pYCbCrRT[2];
	pYCbCrRT[0] = m_pLumaRT;
	pYCbCrRT[1] = m_pChromaRT;

	m_pD3D11DeviceContext->OMSetRenderTargets(2, pYCbCrRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[0], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[1], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pYCbCrShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessYCbCrShader2()
{
	ID3D11RenderTargetView* pYCbCrRT[3];
	pYCbCrRT[0] = m_pLumaRT;
	pYCbCrRT[1] = m_pChromaCBRT;
	pYCbCrRT[2] = m_pChromaCRRT;

	m_pD3D11DeviceContext->OMSetRenderTargets(3, pYCbCrRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[0], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[1], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[2], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pYCbCrShader2, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessYCbCrShader3()
{
	ID3D11RenderTargetView* pYCbCrRT[3];
	pYCbCrRT[0] = m_pI420LumaRT;
	pYCbCrRT[1] = m_pChromaCBRT;
	pYCbCrRT[2] = m_pChromaCRRT;

	m_pD3D11DeviceContext->OMSetRenderTargets(3, pYCbCrRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[0], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[1], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[2], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pYCbCrShader2, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessYChromaShaderForNV12()
{
	ID3D11RenderTargetView* pYCbCrRT[2];
	pYCbCrRT[0] = m_pNV12LumaRT;
	pYCbCrRT[1] = m_pChromaRT;

	m_pD3D11DeviceContext->OMSetRenderTargets(2, pYCbCrRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[0], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[1], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pYCbCrShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessYChromaShaderForI420()
{
	ID3D11RenderTargetView* pYCbCrRT[2];
	pYCbCrRT[0] = m_pI420LumaRT;
	pYCbCrRT[1] = m_pChromaRT;

	m_pD3D11DeviceContext->OMSetRenderTargets(2, pYCbCrRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[0], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[1], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pYCbCrShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessYCbCrShaderMips()
{
	ID3D11RenderTargetView* pYCbCrRT[3];
	pYCbCrRT[0] = m_pLumaRT;
	pYCbCrRT[1] = m_pChromaCBMipsRT;
	pYCbCrRT[2] = m_pChromaCRMipsRT;

	m_pD3D11DeviceContext->OMSetRenderTargets(3, pYCbCrRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[0], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[1], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pYCbCrRT[2], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pYCbCrShader2, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pInputRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessChromaDownSampledShader()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pChromaDownSampledRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pChromaDownSampledRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pChromaRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessChromaDownSampledShaderToNV12()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pNV12ChromaRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pNV12ChromaRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pChromaRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessChromaDownSampledShaderToI420()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pI420ChromaRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pI420ChromaRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pPixelShader4, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pChromaDownSampledRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessChromaDownSampledShader2()
{
	ID3D11RenderTargetView* pChromaRT[2];
	pChromaRT[0] = m_pChromaCBDownSampledRT;
	pChromaRT[1] = m_pChromaCRDownSampledRT;

	ID3D11ShaderResourceView* pChromaRSV[2];
	pChromaRSV[0] = m_pChromaCBRSV;
	pChromaRSV[1] = m_pChromaCRRSV;

	m_pD3D11DeviceContext->OMSetRenderTargets(2, pChromaRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(pChromaRT[0], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->ClearRenderTargetView(pChromaRT[1], DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pPixelShader2, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 2, pChromaRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessYFakeNV12Shader()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pFakeNV12RT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pFakeNV12RT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pLumaRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessYNV12Shader()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pNV12LumaRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pNV12LumaRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pLumaRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessFakeUVShader()
{
	m_pD3D11DeviceContext->VSSetShader(m_pCombinedUVVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pCombinedUVPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pChromaCBDownSampledRSV);
	m_pD3D11DeviceContext->PSSetShaderResources(1, 1, &m_pChromaCRDownSampledRSV);
	m_pD3D11DeviceContext->PSSetShaderResources(2, 1, &m_pShiftWidthRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessFakeUVShaderMips()
{
	m_pD3D11DeviceContext->VSSetShader(m_pCombinedUVVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pCombinedUVMipsPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pChromaCBMipsRSV);
	m_pD3D11DeviceContext->PSSetShaderResources(1, 1, &m_pChromaCRMipsRSV);
	m_pD3D11DeviceContext->PSSetShaderResources(2, 1, &m_pShiftWidthRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

void CD3D11ShaderNV12::ProcessUVShader()
{
	m_pD3D11DeviceContext->OMSetRenderTargets(1, &m_pNV12ChromaRT, NULL);
	m_pD3D11DeviceContext->ClearRenderTargetView(m_pNV12ChromaRT, DirectX::Colors::Aquamarine);
	m_pD3D11DeviceContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShader(m_pChromaCbCrPixelShader, NULL, 0);
	m_pD3D11DeviceContext->PSSetShaderResources(0, 1, &m_pChromaDownSampledRSV);
	m_pD3D11DeviceContext->Draw(4, 0);
	m_pD3D11DeviceContext->Flush();
}

HRESULT CD3D11ShaderNV12::InitVertexPixelShaders()
{
	HRESULT hr = S_OK;

	IF_FAILED_RETURN(InitVertexShaderFromFile(L"ScreenVS.hlsl", &m_pVertexShader, FALSE));
	IF_FAILED_RETURN(InitVertexShaderFromFile(L"CombinedUVVS.hlsl", &m_pCombinedUVVertexShader, TRUE));

	IF_FAILED_RETURN(InitPixelShaderFromFile(L"ScreenPS.hlsl", &m_pPixelShader));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"ScreenPS2.hlsl", &m_pPixelShader2));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"ScreenPS3.hlsl", &m_pPixelShader3));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"ScreenPS4.hlsl", &m_pPixelShader4));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"LumaPS.hlsl", &m_pLumaShader));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"ChromaPS.hlsl", &m_pChromaShader));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"YCbCrPS.hlsl", &m_pYCbCrShader));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"YCbCrPS2.hlsl", &m_pYCbCrShader2));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"CombinedUVPS.hlsl", &m_pCombinedUVPixelShader));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"CombinedUVMipsPS.hlsl", &m_pCombinedUVMipsPixelShader));
	IF_FAILED_RETURN(InitPixelShaderFromFile(L"ChromaCbCrPS.hlsl", &m_pChromaCbCrPixelShader));

	return hr;
}

HRESULT CD3D11ShaderNV12::InitTextures(UINT uiWidth, UINT uiHeight)
{
	HRESULT hr = S_OK;	
	IF_FAILED_RETURN(InitRenderTargetView(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitSmallRenderTargetView(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitShiftWidthTexture(uiWidth));
	IF_FAILED_RETURN(InitRenderTargetLuma(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetChroma(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetChromaCb(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetChromaCr(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetChromaCbMips(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetChromaCrMips(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetChromaDownSampled(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetChromaBDownSampled(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetChromaCDownSampled(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetFakeNV12(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetNV12(uiWidth, uiHeight));
	IF_FAILED_RETURN(InitRenderTargetI420(uiWidth, uiHeight));

	return hr;
}

HRESULT CD3D11ShaderNV12::InitD3D11Resources(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;

	D3D11_SAMPLER_DESC SampDesc;
	ZeroMemory(&SampDesc, sizeof(SampDesc));

	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SampDesc.MinLOD = 0;
	SampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	IF_FAILED_RETURN(m_pD3D11Device->CreateSamplerState(&SampDesc, &m_pSamplerPointState));

	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	IF_FAILED_RETURN(m_pD3D11Device->CreateSamplerState(&SampDesc, &m_pSamplerLinearState));

	InitViewPort(uiWidth, uiHeight);

	m_pD3D11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pD3D11DeviceContext->IASetInputLayout(m_pVertexLayout);
	m_pD3D11DeviceContext->PSSetSamplers(0, 1, &m_pSamplerPointState);

	return hr;
}

void CD3D11ShaderNV12::InitViewPort(const UINT uiWidth, const UINT uiHeight)
{
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)uiWidth;
	vp.Height = (FLOAT)uiHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	m_pD3D11DeviceContext->RSSetViewports(1, &vp);
}

HRESULT CD3D11ShaderNV12::InitVertexShaderFromFile(const WCHAR* wszShaderFile, ID3D11VertexShader** ppID3D11VertexShader, const BOOL bCreateLayout)
{
	HRESULT hr = S_OK;
	ID3DBlob* pVSBlob = NULL;

	try
	{
		IF_FAILED_THROW(CompileShaderFromFile(wszShaderFile, "VS", "vs_5_0", &pVSBlob));
		IF_FAILED_THROW(m_pD3D11Device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, ppID3D11VertexShader));

		if(bCreateLayout)
		{
			D3D11_INPUT_ELEMENT_DESC layout[] = {{"SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},};
			UINT numElements = ARRAYSIZE(layout);

			IF_FAILED_THROW(m_pD3D11Device->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pVertexLayout));
		}
	}
	catch(HRESULT){}

	SAFE_RELEASE(pVSBlob);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitPixelShaderFromFile(const WCHAR* wszShaderFile, ID3D11PixelShader** ppID3D11PixelShader)
{
	HRESULT hr = S_OK;
	ID3DBlob* pPSBlob = NULL;

	try
	{
		IF_FAILED_THROW(CompileShaderFromFile(wszShaderFile, "PS", "ps_5_0", &pPSBlob));
		IF_FAILED_THROW(m_pD3D11Device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, ppID3D11PixelShader));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pPSBlob);

	return hr;
}

HRESULT CD3D11ShaderNV12::CompileShaderFromFile(const WCHAR* wszShaderFile, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;
	ID3DBlob* pErrorBlob = NULL;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef _DEBUG
	dwShaderFlags |= D3DCOMPILE_DEBUG;
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	hr = D3DCompileFromFile(wszShaderFile, NULL, NULL, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);

	if(FAILED(hr) && pErrorBlob)
	{
		OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
	}

	SAFE_RELEASE(pErrorBlob);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetView(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pViewTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pViewTexture2D));
		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pViewTexture2D, NULL, &m_pViewRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pViewTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitSmallRenderTargetView(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pViewTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth / 2;
	desc2D.Height = uiHeight / 2;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pViewTexture2D));
		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pViewTexture2D, NULL, &m_pSmallViewRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pViewTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitShiftWidthTexture(const UINT uiWidth)
{
	HRESULT hr = S_OK;
	ID3D11Texture1D* pInTexture1D = NULL;
	D3D11_SUBRESOURCE_DATA SubResource1D;
	BYTE* pShiftData = NULL;

	D3D11_TEXTURE1D_DESC desc1D;
	desc1D.Width = uiWidth;
	desc1D.MipLevels = 1;
	desc1D.ArraySize = 1;
	desc1D.Format = DXGI_FORMAT_R8_UNORM;
	desc1D.Usage = D3D11_USAGE_DEFAULT;
	desc1D.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc1D.CPUAccessFlags = 0;
	desc1D.MiscFlags = 0;

	try
	{
		pShiftData = new (std::nothrow)BYTE[uiWidth];
		IF_FAILED_THROW(pShiftData == NULL ? E_FAIL : S_OK);

		BYTE* pData = pShiftData;

		for(UINT ui = 0; ui < uiWidth; ui++)
		{
			if(ui % 2)
				*pData++ = 0x01;
			else
				*pData++ = 0x00;
		}

		ZeroMemory(&SubResource1D, sizeof(SubResource1D));
		SubResource1D.pSysMem = (void*)pShiftData;
		SubResource1D.SysMemPitch = uiWidth;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture1D(&desc1D, &SubResource1D, &pInTexture1D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc1D;
		srvDesc1D.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc1D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		srvDesc1D.Texture2D.MipLevels = 1;
		srvDesc1D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pInTexture1D, &srvDesc1D, &m_pShiftWidthRSV));
	}
	catch(HRESULT){}

	SAFE_DELETE_ARRAY(pShiftData);
	SAFE_RELEASE(pInTexture1D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetLuma(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 1;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pLumaRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pLumaRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetChroma(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8G8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8G8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 1;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pChromaRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8G8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pChromaRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetChromaCb(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 1;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pChromaCBRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pChromaCBRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetChromaCr(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 1;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pChromaCRRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pChromaCRRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetChromaCbMips(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 2;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 2;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pChromaCBMipsRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pChromaCBMipsRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetChromaCrMips(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 2;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 2;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pChromaCRMipsRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pChromaCRMipsRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetChromaDownSampled(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth / 2;
	desc2D.Height = uiHeight / 2;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8G8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8G8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 1;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pChromaDownSampledRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8G8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pChromaDownSampledRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetChromaBDownSampled(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth / 2;
	desc2D.Height = uiHeight / 2;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 1;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pChromaCBDownSampledRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pChromaCBDownSampledRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetChromaCDownSampled(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth / 2;
	desc2D.Height = uiHeight / 2;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D;
		srvDesc2D.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc2D.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels = 1;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateShaderResourceView(pTexture2D, &srvDesc2D, &m_pChromaCRDownSampledRSV));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pChromaCRDownSampledRT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetFakeNV12(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pTexture2D = NULL;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight + (uiHeight / 2);
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_R8_UNORM;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pTexture2D));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(pTexture2D, &rtDesc, &m_pFakeNV12RT));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetNV12(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_NV12;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &m_pNV12Texture));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		// D3D11 will map this render target view to the NV12 Luma plane
		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(m_pNV12Texture, &rtDesc, &m_pNV12LumaRT));

		rtDesc.Format = DXGI_FORMAT_R8G8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		// D3D11 will map this render target view to the NV12 Chroma plane(U/V Interleave Plane)
		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(m_pNV12Texture, &rtDesc, &m_pNV12ChromaRT));

	}
	catch(HRESULT){}

	return hr;
}

HRESULT CD3D11ShaderNV12::InitRenderTargetI420(const UINT uiWidth, const UINT uiHeight)
{
	HRESULT hr = S_OK;

	D3D11_TEXTURE2D_DESC desc2D;
	desc2D.Width = uiWidth;
	desc2D.Height = uiHeight;
	desc2D.MipLevels = 1;
	desc2D.ArraySize = 1;
	desc2D.Format = DXGI_FORMAT_NV12;
	desc2D.SampleDesc.Count = 1;
	desc2D.SampleDesc.Quality = 0;
	desc2D.Usage = D3D11_USAGE_DEFAULT;
	desc2D.BindFlags = D3D11_BIND_RENDER_TARGET;
	desc2D.CPUAccessFlags = 0;
	desc2D.MiscFlags = 0;

	try
	{
		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &m_pI420Texture));

		D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
		rtDesc.Format = DXGI_FORMAT_R8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		// D3D11 will map this render target view to the NV12 Luma plane
		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(m_pI420Texture, &rtDesc, &m_pI420LumaRT));

		rtDesc.Format = DXGI_FORMAT_R8G8_UNORM;
		rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtDesc.Texture2D.MipSlice = 0;

		// D3D11 will map this render target view to the NV12 Chroma plane(U/V Interleave Plane)
		IF_FAILED_THROW(m_pD3D11Device->CreateRenderTargetView(m_pI420Texture, &rtDesc, &m_pI420ChromaRT));

	}
	catch (HRESULT) {}

	return hr;
}

HRESULT CD3D11ShaderNV12::CreateBmpFileFromRgbaSurface(ID3D11RenderTargetView* pD3D11RenderTargetView, LPCWSTR wszOutputImageFile)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pInTexture2D = NULL;
	ID3D11Texture2D* pOutTexture2D = NULL;
	D3D11_TEXTURE2D_DESC desc2D;
	D3D11_MAPPED_SUBRESOURCE MappedSubResource;
	UINT uiSubResource = D3D11CalcSubresource(0, 0, 0);
	BYTE* pDataRgb = NULL;

	try
	{
		pD3D11RenderTargetView->GetResource(reinterpret_cast<ID3D11Resource**>(&pInTexture2D));
		pInTexture2D->GetDesc(&desc2D);

		UINT uiImageSize = desc2D.Width * desc2D.Height * 3;

		pDataRgb = new (std::nothrow)BYTE[uiImageSize];
		IF_FAILED_THROW(pDataRgb == NULL ? E_OUTOFMEMORY : S_OK);

		desc2D.BindFlags = 0;
		desc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc2D.Usage = D3D11_USAGE_STAGING;
		desc2D.MiscFlags = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pOutTexture2D));

		m_pD3D11DeviceContext->CopyResource(pOutTexture2D, pInTexture2D);

		IF_FAILED_THROW(m_pD3D11DeviceContext->Map(pOutTexture2D, uiSubResource, D3D11_MAP_READ, 0, &MappedSubResource));

		BYTE* pDataRgbaColor = (BYTE*)MappedSubResource.pData;
		BYTE* pDataRgbColor = pDataRgb;

		for(UINT i = 0; i < desc2D.Height; i++)
		{
			for(UINT j = 0; j < desc2D.Width; j++)
			{
				*pDataRgbColor++ = *pDataRgbaColor++;
				*pDataRgbColor++ = *pDataRgbaColor++;
				*pDataRgbColor++ = *pDataRgbaColor++;
				pDataRgbaColor++;
			}
		}

		m_pD3D11DeviceContext->Unmap(pOutTexture2D, uiSubResource);

		IF_FAILED_THROW(CreateBmpFile(wszOutputImageFile, pDataRgb, uiImageSize, desc2D.Width, desc2D.Height));
	}
	catch(HRESULT){}

	SAFE_DELETE_ARRAY(pDataRgb);
	SAFE_RELEASE(pOutTexture2D);
	SAFE_RELEASE(pInTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::CreateBmpFileFromLumaSurface(ID3D11RenderTargetView* pD3D11RenderTargetView, LPCWSTR wszOutputImageFile)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pInTexture2D = NULL;
	ID3D11Texture2D* pOutTexture2D = NULL;
	D3D11_TEXTURE2D_DESC desc2D;
	D3D11_MAPPED_SUBRESOURCE MappedSubResource;
	UINT uiSubResource = D3D11CalcSubresource(0, 0, 0);
	BYTE* pDataRgb = NULL;

	try
	{
		pD3D11RenderTargetView->GetResource(reinterpret_cast<ID3D11Resource**>(&pInTexture2D));
		pInTexture2D->GetDesc(&desc2D);

		UINT uiImageSize = desc2D.Width * desc2D.Height * 3;

		pDataRgb = new (std::nothrow)BYTE[uiImageSize];
		IF_FAILED_THROW(pDataRgb == NULL ? E_OUTOFMEMORY : S_OK);

		desc2D.BindFlags = 0;
		desc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc2D.Usage = D3D11_USAGE_STAGING;
		desc2D.MiscFlags = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pOutTexture2D));

		m_pD3D11DeviceContext->CopyResource(pOutTexture2D, pInTexture2D);

		IF_FAILED_THROW(m_pD3D11DeviceContext->Map(pOutTexture2D, uiSubResource, D3D11_MAP_READ, 0, &MappedSubResource));

		BYTE* pDataYColor = (BYTE*)MappedSubResource.pData;
		BYTE* pDataRgbColor = pDataRgb;

		for(UINT i = 0; i < desc2D.Height; i++)
		{
			for(UINT j = 0; j < desc2D.Width; j++)
			{
				*pDataRgbColor++ = *pDataYColor;
				*pDataRgbColor++ = *pDataYColor;
				*pDataRgbColor++ = *pDataYColor++;
			}
		}

		m_pD3D11DeviceContext->Unmap(pOutTexture2D, uiSubResource);

		IF_FAILED_THROW(CreateBmpFile(wszOutputImageFile, pDataRgb, uiImageSize, desc2D.Width, desc2D.Height));
	}
	catch(HRESULT){}

	SAFE_DELETE_ARRAY(pDataRgb);
	SAFE_RELEASE(pOutTexture2D);
	SAFE_RELEASE(pInTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::CreateBmpFileFromLumaSurface(ID3D11ShaderResourceView* pD3D11ShaderResourceView, LPCWSTR wszOutputImageFile)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pInTexture2D = NULL;
	ID3D11Texture2D* pOutTexture2D = NULL;
	D3D11_TEXTURE2D_DESC desc2D;
	D3D11_MAPPED_SUBRESOURCE MappedSubResource;
	UINT uiSubResource = D3D11CalcSubresource(1, 0, 0);
	BYTE* pDataRgb = NULL;

	try
	{
		pD3D11ShaderResourceView->GetResource(reinterpret_cast<ID3D11Resource**>(&pInTexture2D));
		pInTexture2D->GetDesc(&desc2D);

		UINT uiWidth = desc2D.Width / 2;
		UINT uiHeight = desc2D.Height / 2;
		UINT uiImageSize = uiWidth * uiHeight * 3;

		pDataRgb = new (std::nothrow)BYTE[uiImageSize];
		IF_FAILED_THROW(pDataRgb == NULL ? E_OUTOFMEMORY : S_OK);

		desc2D.BindFlags = 0;
		desc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc2D.Usage = D3D11_USAGE_STAGING;
		desc2D.MiscFlags = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pOutTexture2D));

		m_pD3D11DeviceContext->CopyResource(pOutTexture2D, pInTexture2D);

		IF_FAILED_THROW(m_pD3D11DeviceContext->Map(pOutTexture2D, uiSubResource, D3D11_MAP_READ, 0, &MappedSubResource));

		BYTE* pDataYColor = (BYTE*)MappedSubResource.pData;
		BYTE* pDataRgbColor = pDataRgb;

		for(UINT i = 0; i < uiHeight; i++)
		{
			for(UINT j = 0; j < uiWidth; j++)
			{
				*pDataRgbColor++ = *pDataYColor;
				*pDataRgbColor++ = *pDataYColor;
				*pDataRgbColor++ = *pDataYColor++;
			}
		}

		m_pD3D11DeviceContext->Unmap(pOutTexture2D, uiSubResource);

		IF_FAILED_THROW(CreateBmpFile(wszOutputImageFile, pDataRgb, uiImageSize, uiWidth, uiHeight));
	}
	catch(HRESULT){}

	SAFE_DELETE_ARRAY(pDataRgb);
	SAFE_RELEASE(pOutTexture2D);
	SAFE_RELEASE(pInTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::CreateBmpFileFromChromaSurface(ID3D11RenderTargetView* pD3D11RenderTargetView, LPCWSTR wszOutputImageFile, const BOOL bU)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pInTexture2D = NULL;
	ID3D11Texture2D* pOutTexture2D = NULL;
	D3D11_TEXTURE2D_DESC desc2D;
	D3D11_MAPPED_SUBRESOURCE MappedSubResource;
	UINT uiSubResource = D3D11CalcSubresource(0, 0, 0);
	BYTE* pDataRgb = NULL;

	try
	{
		pD3D11RenderTargetView->GetResource(reinterpret_cast<ID3D11Resource**>(&pInTexture2D));
		pInTexture2D->GetDesc(&desc2D);

		UINT uiImageSize = desc2D.Width * desc2D.Height * 3;

		pDataRgb = new (std::nothrow)BYTE[uiImageSize];
		IF_FAILED_THROW(pDataRgb == NULL ? E_OUTOFMEMORY : S_OK);

		desc2D.BindFlags = 0;
		desc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc2D.Usage = D3D11_USAGE_STAGING;
		desc2D.MiscFlags = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pOutTexture2D));

		m_pD3D11DeviceContext->CopyResource(pOutTexture2D, pInTexture2D);

		IF_FAILED_THROW(m_pD3D11DeviceContext->Map(pOutTexture2D, uiSubResource, D3D11_MAP_READ, 0, &MappedSubResource));

		BYTE* pDataUVColor = (BYTE*)MappedSubResource.pData;
		BYTE* pDataRgbColor = pDataRgb;

		for(UINT i = 0; i < desc2D.Height; i++)
		{
			for(UINT j = 0; j < desc2D.Width; j++)
			{
				if(bU == TRUE)
				{
					// Show U
					*pDataRgbColor++ = *pDataUVColor;
					*pDataRgbColor++ = *pDataUVColor;
					*pDataRgbColor++ = *pDataUVColor++;
					pDataUVColor++;
				}
				else
				{
					// Show V
					pDataUVColor++;
					*pDataRgbColor++ = *pDataUVColor;
					*pDataRgbColor++ = *pDataUVColor;
					*pDataRgbColor++ = *pDataUVColor++;
				}
				
			}
		}

		m_pD3D11DeviceContext->Unmap(pOutTexture2D, uiSubResource);

		IF_FAILED_THROW(CreateBmpFile(wszOutputImageFile, pDataRgb, uiImageSize, desc2D.Width, desc2D.Height));
	}
	catch(HRESULT){}

	SAFE_DELETE_ARRAY(pDataRgb);
	SAFE_RELEASE(pOutTexture2D);
	SAFE_RELEASE(pInTexture2D);

	return hr;
}

HRESULT CD3D11ShaderNV12::CreateBmpFileFromLumaChromaDownSampledSurface(ID3D11RenderTargetView* pLumaRT, ID3D11RenderTargetView* pChromaRT, LPCWSTR wszOutputImageFile)
{
	HRESULT hr = S_OK;
	ID3D11Texture2D* pLumaTexture = NULL;
	ID3D11Texture2D* pChromaTexture = NULL;
	D3D11_TEXTURE2D_DESC descLuma2D;
	D3D11_TEXTURE2D_DESC descChroma2D;
	BYTE* pNV12Data = NULL;
	ID3D11Texture2D* pLumaStagingTexture = NULL;
	ID3D11Texture2D* pChromaStagingTexture = NULL;
	D3D11_MAPPED_SUBRESOURCE MappedSubResource;
	UINT uiSubResource = D3D11CalcSubresource(0, 0, 0);

	try
	{
		pLumaRT->GetResource(reinterpret_cast<ID3D11Resource**>(&pLumaTexture));
		pChromaRT->GetResource(reinterpret_cast<ID3D11Resource**>(&pChromaTexture));

		pLumaTexture->GetDesc(&descLuma2D);
		pChromaTexture->GetDesc(&descChroma2D);

		UINT uiImageSize = descLuma2D.Width * (descLuma2D.Height + (descLuma2D.Height / 2));

		pNV12Data = new (std::nothrow)BYTE[uiImageSize];
		IF_FAILED_THROW(pNV12Data == NULL ? E_OUTOFMEMORY : S_OK);

		descLuma2D.BindFlags = 0;
		descLuma2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		descLuma2D.Usage = D3D11_USAGE_STAGING;
		descLuma2D.MiscFlags = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&descLuma2D, NULL, &pLumaStagingTexture));

		descChroma2D.BindFlags = 0;
		descChroma2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		descChroma2D.Usage = D3D11_USAGE_STAGING;
		descChroma2D.MiscFlags = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&descChroma2D, NULL, &pChromaStagingTexture));

		// Luma
		m_pD3D11DeviceContext->CopyResource(pLumaStagingTexture, pLumaTexture);

		IF_FAILED_THROW(m_pD3D11DeviceContext->Map(pLumaStagingTexture, uiSubResource, D3D11_MAP_READ, 0, &MappedSubResource));

		UINT uiSize = descLuma2D.Width * descLuma2D.Height;
		memcpy(pNV12Data, MappedSubResource.pData, uiSize);

		m_pD3D11DeviceContext->Unmap(pLumaStagingTexture, uiSubResource);

		// Chroma
		m_pD3D11DeviceContext->CopyResource(pChromaStagingTexture, pChromaTexture);

		IF_FAILED_THROW(m_pD3D11DeviceContext->Map(pChromaStagingTexture, uiSubResource, D3D11_MAP_READ, 0, &MappedSubResource));

		memcpy(pNV12Data + uiSize, MappedSubResource.pData, (descChroma2D.Width * 2) * descChroma2D.Height);

		m_pD3D11DeviceContext->Unmap(pChromaStagingTexture, uiSubResource);

		IF_FAILED_THROW(ProcessNV12ToBmpFile(wszOutputImageFile, pNV12Data, descLuma2D.Width, descLuma2D.Width, descLuma2D.Height));
	}
	catch(HRESULT){}

	SAFE_RELEASE(pChromaStagingTexture);
	SAFE_RELEASE(pLumaStagingTexture);
	SAFE_DELETE_ARRAY(pNV12Data);
	SAFE_RELEASE(pChromaTexture);
	SAFE_RELEASE(pLumaTexture);

	return hr;
}

HRESULT CD3D11ShaderNV12::CreateBmpFileFromNV12Surface(ID3D11RenderTargetView* pNV12RT, LPCWSTR wszOutputImageFile)
{
	HRESULT hr = S_OK;
	D3D11_TEXTURE2D_DESC desc2D;
	ID3D11Texture2D* pNV12Texture = NULL;
	ID3D11Texture2D* pNV12StagingTexture = NULL;
	D3D11_MAPPED_SUBRESOURCE MappedSubResource;
	UINT uiSubResource = D3D11CalcSubresource(0, 0, 0);

	try
	{
		pNV12RT->GetResource(reinterpret_cast<ID3D11Resource**>(&pNV12Texture));
		pNV12Texture->GetDesc(&desc2D);

		desc2D.BindFlags = 0;
		desc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc2D.Usage = D3D11_USAGE_STAGING;
		desc2D.MiscFlags = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pNV12StagingTexture));

		m_pD3D11DeviceContext->CopyResource(pNV12StagingTexture, pNV12Texture);

		IF_FAILED_THROW(m_pD3D11DeviceContext->Map(pNV12StagingTexture, uiSubResource, D3D11_MAP_READ, 0, &MappedSubResource));

		hr = ProcessNV12ToBmpFile(wszOutputImageFile, (BYTE*)MappedSubResource.pData, MappedSubResource.RowPitch, desc2D.Width, (desc2D.Height * 2) / 3);

		m_pD3D11DeviceContext->Unmap(pNV12StagingTexture, uiSubResource);
	}
	catch(HRESULT){}

	SAFE_RELEASE(pNV12StagingTexture);
	SAFE_RELEASE(pNV12Texture);

	return hr;
}

HRESULT CD3D11ShaderNV12::CreateBmpFileFromNV12Texture(ID3D11Texture2D* pNV12Texture, LPCWSTR wszOutputImageFile)
{
	HRESULT hr = S_OK;
	D3D11_TEXTURE2D_DESC desc2D;
	ID3D11Texture2D* pNV12StagingTexture = NULL;
	D3D11_MAPPED_SUBRESOURCE MappedSubResource;
	UINT uiSubResource = D3D11CalcSubresource(0, 0, 0);

	try
	{
		pNV12Texture->GetDesc(&desc2D);

		desc2D.BindFlags = 0;
		desc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc2D.Usage = D3D11_USAGE_STAGING;
		desc2D.MiscFlags = 0;

		IF_FAILED_THROW(m_pD3D11Device->CreateTexture2D(&desc2D, NULL, &pNV12StagingTexture));

		m_pD3D11DeviceContext->CopyResource(pNV12StagingTexture, pNV12Texture);

		IF_FAILED_THROW(m_pD3D11DeviceContext->Map(pNV12StagingTexture, uiSubResource, D3D11_MAP_READ, 0, &MappedSubResource));

		hr = ProcessNV12ToBmpFile(wszOutputImageFile, (BYTE*)MappedSubResource.pData, MappedSubResource.RowPitch, desc2D.Width, desc2D.Height);

		m_pD3D11DeviceContext->Unmap(pNV12StagingTexture, uiSubResource);
	}
	catch (HRESULT) {}

	SAFE_RELEASE(pNV12StagingTexture);

	return hr;
}

void CD3D11ShaderNV12::SaveNV12(ID3D11Texture2D* pTexture2D, const char* nv12_filename, uint32_t image_width, uint32_t image_height)
{
	D3D11_TEXTURE2D_DESC desc;
	ComPtr<ID3D11Texture2D> spTemp;
	ComPtr<IDXGISurface> spSurface;

	pTexture2D->GetDesc(&desc);
	//desc.Format = DXGI_FORMAT_NV12;
	desc.ArraySize = 1;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;

	if (SUCCEEDED(m_pD3D11Device->CreateTexture2D(&desc, NULL, &spTemp)))
	{
		m_pD3D11DeviceContext->CopyResource(spTemp.Get(), pTexture2D);

		if (SUCCEEDED(spTemp.As(&spSurface)))
		{
			FILE *pFile = NULL;
			fopen_s(&pFile, nv12_filename, "wb");

			if (pFile != NULL) {
				DXGI_MAPPED_RECT map;
				if (SUCCEEDED(spSurface->Map(&map, DXGI_MAP_READ)))
				{
					uint32_t h = image_height;
					if (h > desc.Height)
						h = desc.Height;
					uint32_t w = image_width;
					if (w > (uint32_t)map.Pitch)
						w = map.Pitch;

					BYTE* pNV12 = map.pBits;
					if (desc.Format == DXGI_FORMAT_NV12 || desc.Format == DXGI_FORMAT_R8_UNORM) {
						// Copy Y plane
						for (uint32_t lineidx = 0; lineidx < h; lineidx++)
							fwrite(pNV12 + (size_t)map.Pitch * lineidx, 1, w, pFile);
					}

					if (desc.Format == DXGI_FORMAT_NV12)
					{
						// Copy UV plane
						pNV12 += (size_t)map.Pitch*desc.Height;
						for (uint32_t lineidx = 0; lineidx < h / 2; lineidx++)
							fwrite(pNV12 + (size_t)map.Pitch * lineidx, 1, w, pFile);
					}

					spSurface->Unmap();
				}

				fclose(pFile);
			}
		}
	}

	return;
}

void CD3D11ShaderNV12::SaveI420(ID3D11Texture2D* pTexture2D, const char* nv12_filename, uint32_t image_width, uint32_t image_height)
{
	D3D11_TEXTURE2D_DESC desc;
	ComPtr<ID3D11Texture2D> spTemp;
	ComPtr<IDXGISurface> spSurface;

	pTexture2D->GetDesc(&desc);
	//desc.Format = DXGI_FORMAT_NV12;
	desc.ArraySize = 1;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;

	assert(image_width % 2 == 0 && image_height % 2 == 0);

	if (SUCCEEDED(m_pD3D11Device->CreateTexture2D(&desc, NULL, &spTemp)))
	{
		m_pD3D11DeviceContext->CopyResource(spTemp.Get(), pTexture2D);

		if (SUCCEEDED(spTemp.As(&spSurface)))
		{
			FILE *pFile = NULL;
			fopen_s(&pFile, nv12_filename, "wb");

			if (pFile != NULL) {
				DXGI_MAPPED_RECT map;
				if (SUCCEEDED(spSurface->Map(&map, DXGI_MAP_READ)))
				{
					uint32_t h = image_height;
					if (h > desc.Height)
						h = desc.Height;
					uint32_t w = image_width;
					if (w > (uint32_t)map.Pitch)
						w = map.Pitch;

					BYTE* pI420 = map.pBits;
					if (desc.Format == DXGI_FORMAT_NV12 || desc.Format == DXGI_FORMAT_R8_UNORM) {
						// Copy Y plane
						for (uint32_t lineidx = 0; lineidx < h; lineidx++)
							fwrite(pI420 + (size_t)map.Pitch * lineidx, 1, w, pFile);
					}

					// Copy U plane
					// The required U data w/2*h/2
					pI420 += (size_t)map.Pitch*desc.Height;
					for (uint32_t lineidx = 0; lineidx < h / 4; lineidx++)
						fwrite(pI420 + (size_t)map.Pitch * lineidx, 1, w, pFile);

					if (w*h / 4 > h / 4 * w)
						fwrite(pI420 + (size_t)h / 4 * map.Pitch, 1, w*h / 4 - h / 4 * w, pFile);

					// Copy V plane
					pI420 += (size_t)h / 4 * map.Pitch + w * h / 4 - h / 4 * w;
					for (uint32_t lineidx = 0; lineidx < h / 4; lineidx++)
						fwrite(pI420 + (size_t)map.Pitch * lineidx, 1, w, pFile);

					if (w*h / 4 > h / 4 * w)
						fwrite(pI420 + (size_t)h / 4 * map.Pitch, 1, w*h / 4 - h / 4 * w, pFile);

					spSurface->Unmap();
				}

				fclose(pFile);
			}
		}
	}

	return;
}

void SaveToBMP(ID3D11Device *device, ID3D11Texture2D* pTexture2D, const char* bmp_filename)
{
	HRESULT hr = S_OK;

	D3D11_TEXTURE2D_DESC desc;
	ComPtr<ID3D11Texture2D>  spTemp;
	ComPtr<ID3D11DeviceContext> spDeviceContext;
	ComPtr<IDXGISurface> spSurface;

	device->GetImmediateContext(&spDeviceContext);

	pTexture2D->GetDesc(&desc);
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.ArraySize = 1;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;

	if (SUCCEEDED(device->CreateTexture2D(&desc, NULL, &spTemp)))
	{
		spDeviceContext->CopyResource(spTemp.Get(), pTexture2D);

		if (SUCCEEDED(hr = spTemp.As(&spSurface)))
		{
			FILE *pFile = NULL;
			fopen_s(&pFile, bmp_filename, "wb");

			if (pFile != NULL) {
				DXGI_MAPPED_RECT map;
				spSurface->Map(&map, DXGI_MAP_READ);

				BITMAPFILEHEADER fh;
				BITMAPINFOHEADER bi;
				LONG data = desc.Width * desc.Height * 4;
				ZeroMemory(reinterpret_cast<void*>(&fh), sizeof(BITMAPFILEHEADER));
				ZeroMemory(reinterpret_cast<void*>(&bi), sizeof(BITMAPINFOHEADER));
				fh.bfType = 0x4d42;
				fh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + data;
				fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
				bi.biBitCount = 32;
				bi.biSizeImage = data;
				bi.biPlanes = 1;
				bi.biCompression = 0;
				bi.biSize = sizeof(BITMAPINFOHEADER);
				bi.biWidth = desc.Width;
				bi.biHeight = -1 * desc.Height;
				fwrite(&fh, sizeof(BITMAPFILEHEADER), 1, pFile);
				fwrite(&bi, sizeof(BITMAPINFOHEADER), 1, pFile);

				for (UINT i = 0; i < desc.Height; i++)
					fwrite(map.pBits + (LONGLONG)i * map.Pitch, sizeof(BYTE), desc.Width * 4, pFile);

				fclose(pFile);
				spSurface->Unmap();
			}
		}
	}

	return;
}
