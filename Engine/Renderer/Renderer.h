#pragma once

#include "ShadowMapRenderPass.h"
#include "GBufferRenderPass.h"
#include "DeferredShadingRenderPass.h"
#include "VoxelInjectionRenderPass.h"
#include "SHIndirectRenderPass.h"
#include "IndirectDiffuseLightingRenderPass.h"
#include "IndirectSpecularLightingRenderPass.h"
#include "SkyBoxRenderPass.h"
#include "FXAARenderPass.h"
#include "ToneMappingRenderPass.h"
#include "ColorGradingRenderPass.h"

class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;

	static void Initialize(ComPtr<ID3D12Device>, int, int, DXGI_FORMAT, DXGI_FORMAT);
	static void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*, ID3D12Resource*, ID3D12Resource*, ID3D12Resource*);
	static void CopyToBackBuffer(ID3D12GraphicsCommandList*, ID3D12Resource*);

	static ShadowMapRenderPass shadowMapRenderPass;
	static GBufferRenderPass gBufferRenderPass;
	static DeferredShadingRenderPass deferredShadingRenderPass;
	static VoxelInjectionRenderPass voxelInjectionRenderPass;
	static SHIndirectRenderPass shIndirectRenderPass;
	static IndirectDiffuseLightingRenderPass indirectDiffuseLightingRenderPass;
	static IndirectSpecularLightingRenderPass indirectSpecularLightingRenderPass;
	static SkyBoxRenderPass skyBoxRenderPass;
	static FXAARenderPass fxaaRenderPass;
	static ToneMappingRenderPass toneMappingRenderPass;
	static ColorGradingRenderPass colorGradingRenderPass;
};