//--------------------------------------------------------------------------------------
// ScreenPS2.hlsl
//--------------------------------------------------------------------------------------
//Texture2D txInput : register(t0);
Texture2D txInputU : register(t0);
Texture2D txInputV : register(t1);

SamplerState GenericSampler : register(s0);

struct PS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float2 PS(PS_INPUT input) : SV_Target
{
	float2 output;

	/*
	Texture2D tex = txInput;
	float2 size = 0;
	float miplevels;
	tex.GetDimensions(0, size.x, size.y, miplevels); // Whole texture size, not UV plane size

	float2 uv_pos = input.Tex*size;

	uint plane_idx = uv_pos.y < size.y / 2 ? 0 : 1;

	float pick_sample1_1D = (uv_pos.x + (uv_pos.y - plane_idx*size.y/2) * size.x) * 2;
	float pick_sample2_1D = (uv_pos.x + (uv_pos.y - plane_idx*size.y/2) * size.x) * 2 + 1;
	float2 pick_sample1_pos_tex = float2(pick_sample1_1D%size.x / size.x, pick_sample1_1D / size.x / size.y);
	float2 pick_sample2_pos_tex = float2(pick_sample2_1D%size.x / size.x, pick_sample2_1D / size.x / size.y);

	output.x = txInput.Sample(GenericSampler, pick_sample1_pos_tex)[plane_idx];
	output.y = txInput.Sample(GenericSampler, pick_sample2_pos_tex)[plane_idx];
	*/

	Texture2D tex = txInputU;
	float2 size = 0;
	float miplevels;
	tex.GetDimensions(0, size.x, size.y, miplevels); // Whole texture size, not UV plane size

	float2 uv_pos = input.Tex*size;

	uint plane_idx = uv_pos.y < size.y / 2 ? 0 : 1;

	float pick_sample1_1D = (uv_pos.x + (uv_pos.y - plane_idx * size.y / 2) * size.x) * 2;
	float pick_sample2_1D = (uv_pos.x + (uv_pos.y - plane_idx * size.y / 2) * size.x) * 2 + 1;
	float2 pick_sample1_pos_tex = float2(pick_sample1_1D%size.x / size.x, pick_sample1_1D / size.x / size.y);
	float2 pick_sample2_pos_tex = float2(pick_sample2_1D%size.x / size.x, pick_sample2_1D / size.x / size.y);

	if (plane_idx == 0)
	{
		output.x = txInputU.Sample(GenericSampler, pick_sample1_pos_tex);
		output.y = txInputU.Sample(GenericSampler, pick_sample2_pos_tex);
	}
	
	if (plane_idx == 1)
	{
		output.x = txInputV.Sample(GenericSampler, pick_sample1_pos_tex);
		output.y = txInputV.Sample(GenericSampler, pick_sample2_pos_tex);
	}

	//output.x = 1.0*uv_pos.y / 858;	//txInputU.Sample(GenericSampler, pick_sample1_pos_tex);
	//output.y = 1.0*uv_pos.y / 858;	//txInputU.Sample(GenericSampler, pick_sample2_pos_tex);

	return output;
}