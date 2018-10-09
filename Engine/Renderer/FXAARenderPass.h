#pragma once

#include "RenderPass.h"

class FXAARenderPass : public RenderPass
{
public:
	FXAARenderPass() = default;
	virtual void Execute(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE*,
		ID3D12Resource*, ID3D12Resource*, ID3D12Resource*) override;
	~FXAARenderPass() = default;

protected:
	virtual void BuildRootSignature() override;
	virtual void BuildDescriptorHeaps() override;
	virtual void BuildPSOs() override;
	virtual void Draw(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*) override;
};