#include "Renderer.h"

ShadowMapRenderPass Renderer::shadowMapRenderPass;
GBufferRenderPass Renderer::gBufferRenderPass;
DeferredShadingRenderPass Renderer::deferredShadingRenderPass;
VoxelInjectionRenderPass Renderer::voxelInjectionFirstBounceRenderPass;
SHIndirectRenderPass Renderer::shIndirectFirstBounceRenderPass;
IndirectLightingRenderPass Renderer::indirectLightingFirstBounceRenderPass;
VoxelInjectionRenderPass Renderer::voxelInjectionSecondBounceRenderPass;
SHIndirectRenderPass Renderer::shIndirectSecondBounceRenderPass;
IndirectLightingRenderPass Renderer::indirectLightingSecondBounceRenderPass;
SkyBoxRenderPass Renderer::skyBoxRenderPass;
FXAARenderPass Renderer::fxaaRenderPass;
ToneMappingRenderPass Renderer::toneMappingRenderPass;
ColorGradingRenderPass Renderer::colorGradingRenderPass;

void Renderer::Initialize(ComPtr<ID3D12Device> inputDevice, int inputWidth, int inputHeight,
	DXGI_FORMAT inputFormatBackBuffer, DXGI_FORMAT inputFormatDepthBuffer)
{
	shadowMapRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr, nullptr, nullptr, L"ShadowMap.hlsl", L"");
	
	gBufferRenderPass.Initialize(inputDevice, inputWidth, inputHeight, 
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr, nullptr, nullptr, L"GBufferWrite.hlsl", L"");
	
	deferredShadingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, shadowMapRenderPass.mOutputBuffers, 
		gBufferRenderPass.mOutputBuffers, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"DeferredShading.hlsl", L"");
	
	voxelInjectionFirstBounceRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, deferredShadingRenderPass.mOutputBuffers, 
		gBufferRenderPass.mOutputBuffers, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"", L"VoxelInjection.hlsl", true);
	
	shIndirectFirstBounceRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr, 
		voxelInjectionFirstBounceRenderPass.mOutputBuffers, gBufferRenderPass.mDepthStencilBuffer, L"", L"SHIndirectConeTracing.hlsl", true);

	indirectLightingFirstBounceRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, deferredShadingRenderPass.mOutputBuffers, 
		gBufferRenderPass.mOutputBuffers, shIndirectFirstBounceRenderPass.mOutputBuffers, gBufferRenderPass.mDepthStencilBuffer, 
		L"IndirectLighting.hlsl", L"");

	voxelInjectionSecondBounceRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, indirectLightingFirstBounceRenderPass.mOutputBuffers,
		gBufferRenderPass.mOutputBuffers, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"", L"VoxelInjection.hlsl", true);

	shIndirectSecondBounceRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, nullptr, nullptr,
		voxelInjectionSecondBounceRenderPass.mOutputBuffers, gBufferRenderPass.mDepthStencilBuffer, L"", L"SHIndirectConeTracing.hlsl", true);

	indirectLightingSecondBounceRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, indirectLightingFirstBounceRenderPass.mOutputBuffers,
		gBufferRenderPass.mOutputBuffers, shIndirectSecondBounceRenderPass.mOutputBuffers, gBufferRenderPass.mDepthStencilBuffer,
		L"IndirectLighting.hlsl", L"");

	skyBoxRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, indirectLightingSecondBounceRenderPass.mOutputBuffers,
		nullptr, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"SkyBox.hlsl", L"");

	toneMappingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, skyBoxRenderPass.mOutputBuffers,
		nullptr, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"ToneMapping.hlsl", L"");

	fxaaRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, toneMappingRenderPass.mOutputBuffers,
		nullptr, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"FXAA.hlsl", L"");
	
	colorGradingRenderPass.Initialize(inputDevice, inputWidth, inputHeight,
		inputFormatBackBuffer, inputFormatDepthBuffer, fxaaRenderPass.mOutputBuffers, 
		nullptr, nullptr, gBufferRenderPass.mDepthStencilBuffer, L"ColorGrading.hlsl", L"");
}

void Renderer::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE * depthStencilViewPtr,
	ID3D12Resource * passCB, ID3D12Resource * objectCB, ID3D12Resource * matCB)
{
	// Render the gBuffers
	gBufferRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Compute the lighting using deferred shading
	deferredShadingRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Inject lighting data into the voxel grids
	voxelInjectionFirstBounceRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Cone trace indirect lighting and inject it into the spherical harmonic grids
	shIndirectFirstBounceRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Sample SH grid to compute indirect lighting
	indirectLightingFirstBounceRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Inject lighting data into the voxel grids
	voxelInjectionSecondBounceRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Cone trace indirect lighting and inject it into the spherical harmonic grids
	shIndirectSecondBounceRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Sample SH grid to compute indirect lighting
	indirectLightingSecondBounceRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Render skybox on the background pixels using a quad
	skyBoxRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Perform anti-aliasing using FXAA
	fxaaRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Bring the texture down to LDR range from HDR using Uncharted 2 style tonemapping
	toneMappingRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);

	// Use 2D LUTs for color grading
	colorGradingRenderPass.Execute(commandList, depthStencilViewPtr, passCB, objectCB, matCB);
}

void Renderer::CopyToBackBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource * backBuffer)
{
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(colorGradingRenderPass.mOutputBuffers[0].Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));

	commandList->CopyResource(backBuffer, Renderer::colorGradingRenderPass.mOutputBuffers[0].Get());

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(colorGradingRenderPass.mOutputBuffers[0].Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ));
}
