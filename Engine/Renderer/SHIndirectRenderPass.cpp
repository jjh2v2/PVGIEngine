#include "SHIndirectRenderPass.h"

void SHIndirectRenderPass::Execute(ID3D12GraphicsCommandList * commandList, D3D12_CPU_DESCRIPTOR_HANDLE * depthStencilViewPtr,
	ID3D12Resource * passCB, ID3D12Resource * objectCB, ID3D12Resource * matCB)
{
	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	commandList->SetPipelineState(mPSO.Get());

	ID3D12DescriptorHeap* descriptorHeapsToneMapping[] = { mSrvDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeapsToneMapping), descriptorHeapsToneMapping);

	for (int i = 0; i < 3; ++i)
	{
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffers[i].Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}

	float lengthOfCone = (32.0f * worldVolumeBoundary) / ((voxelResolution / 2) * tan(MathHelper::Pi / 6.0f));
	float coneStep = lengthOfCone / 64.0f;
	
	commandList->SetComputeRootSignature(mRootSignature.Get());

	commandList->SetComputeRoot32BitConstants(0, 1, &gridResolution, 0);
	commandList->SetComputeRoot32BitConstants(0, 1, &worldVolumeBoundary, 1);
	commandList->SetComputeRoot32BitConstants(0, 1, &coneStep, 2);

	commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	commandList->SetComputeRootDescriptorTable(2, tex);

	tex.Offset(6, cbvSrvUavDescriptorSize);

	commandList->SetComputeRootDescriptorTable(3, tex);

	commandList->Dispatch(gridResolution / 2, gridResolution / 2, gridResolution / 2);

	for (int i = 0; i < 3; ++i)
	{
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffers[i].Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
	}
}

void SHIndirectRenderPass::Draw(ID3D12GraphicsCommandList *, ID3D12Resource *, ID3D12Resource *)
{
}

void SHIndirectRenderPass::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable0;
	uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstants(3, 0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsDescriptorTable(1, &srvTable0);
	slotRootParameter[3].InitAsDescriptorTable(1, &uavTable0);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void SHIndirectRenderPass::BuildDescriptorHeaps()
{
	mOutputBuffers = new ComPtr<ID3D12Resource>[3];

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	texDesc.Alignment = 0;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_CLEAR_VALUE clearVal;
	clearVal.Color[0] = 0.0f;
	clearVal.Color[1] = 0.0f;
	clearVal.Color[2] = 0.0f;
	clearVal.Color[3] = 0.0f;
	clearVal.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	texDesc.Width = gridResolution;
	texDesc.Height = gridResolution;
	texDesc.DepthOrArraySize = gridResolution;

	for (int i = 0; i < 3; ++i)
	{
		ThrowIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mOutputBuffers[i])));
	}
	
	//
	// Create the SRV and UAV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 9;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with texture descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	UINT cbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescVoxelGrid = {};
	srvDescVoxelGrid.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescVoxelGrid.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	srvDescVoxelGrid.Texture3D.MostDetailedMip = 0;
	srvDescVoxelGrid.Texture3D.ResourceMinLODClamp = 0.0f;
	srvDescVoxelGrid.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDescVoxelGrid.Texture3D.MipLevels = 1;

	for (int i = 0; i < 6; ++i)
	{
		md3dDevice->CreateShaderResourceView(mVoxelGrids[i].Get(), &srvDescVoxelGrid, hDescriptor);

		hDescriptor.Offset(1, cbvSrvUavDescriptorSize);
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = gridResolution;

	for (int i = 0; i < 3; ++i)
	{
		md3dDevice->CreateUnorderedAccessView(mOutputBuffers[i].Get(), nullptr, &uavDesc, hDescriptor);

		hDescriptor.Offset(1, cbvSrvUavDescriptorSize);
	}
}

void SHIndirectRenderPass::BuildPSOs()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};

	computePSODesc.pRootSignature = mRootSignature.Get();
	computePSODesc.CS =
	{
		reinterpret_cast<BYTE*>(mComputeShader->GetBufferPointer()),
		mComputeShader->GetBufferSize()
	};
	computePSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(&mPSO)));
}