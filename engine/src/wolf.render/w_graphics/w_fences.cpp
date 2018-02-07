#include "w_render_pch.h"
#include "w_graphics_device_manager.h"
#include "w_fences.h"

using namespace wolf::graphics;

HRESULT w_fences::initialize(_In_ const std::shared_ptr<w_graphics_device>& pGDevice, _In_ const uint32_t pNumberOfFences)
{
    HRESULT _hr = S_OK;
    
    this->_gDevice = pGDevice;

    if (this->_fences.size())
    {
        release();
    }
    
#ifdef __VULKAN__
    //Fence for render sync
    VkFenceCreateInfo _fence_create_info = {};
    _fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    _fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (size_t i = 0; i < pNumberOfFences; ++i)
    {
        VkFence _fence = 0;
        if (vkCreateFence(pGDevice->vk_device,
                      &_fence_create_info,
                      nullptr,
                      &_fence))
        {
            V(S_FALSE, "creating fence", "w_fence", 3, false);
            _hr = S_FALSE;
        }
        else
        {
            this->_fences.push_back(_fence);
        }
    }
#elif defined(__DX12__)
    
#endif
    
    return _hr;
}

HRESULT w_fences::wait(_In_ uint64_t pTimeOut)
{
    return vkWaitForFences(this->_gDevice->vk_device, 1, this->_fences.data(), VK_TRUE, pTimeOut) == VkResult::VK_SUCCESS ? S_OK : S_FALSE;
}

HRESULT w_fences::reset()
{
    //reset fence
#ifdef __VULKAN__
    return vkResetFences(this->_gDevice->vk_device, 1, this->_fences.data()) == VkResult::VK_SUCCESS ? S_OK : S_FALSE;
#elif defined(__DX12__)
    
#endif
}

VkFence* w_fences::get()
{
    if (this->_fences.size() == 0)  return nullptr;
    return &(this->_fences.at(0));
}

VkFence* w_fences::get_all()
{
    if (this->_fences.size() == 0)  return nullptr;
    return this->_fences.data();
}

uint32_t w_fences::get_count()
{
    return static_cast<uint32_t>(this->_fences.size());
}

ULONG w_fences::release()
{
#ifdef __VULKAN__
    for (auto& _fence : this->_fences)
    {
        vkDestroyFence(this->_gDevice->vk_device, _fence, nullptr);
    }
#elif defined(__DX12__)
    
#endif
    
    this->_fences.clear();
    this->_gDevice = nullptr;

    return 0;
}

#ifdef __PYTHON__

bool w_fences::py_initialize(_In_ boost::shared_ptr<wolf::graphics::w_graphics_device>& pGDevice, _In_ const uint32_t& pNumberOfFences)
{
	//create render pass attchaments
	if (!pGDevice.get()) return false;
	
	auto _gDevice = boost_shared_ptr_to_std_shared_ptr<w_graphics_device>(pGDevice);
	auto _hr = this->initialize(_gDevice, pNumberOfFences);
	_gDevice.reset();
	
	return _hr == S_OK;
}

bool w_fences::py_wait_for(_In_ uint64_t& pTimeOut)
{
	return wait(pTimeOut) == S_OK;
}

bool w_fences::py_wait()
{
	return wait() == S_OK;
}

bool w_fences::py_reset()
{
	return reset() == S_OK;
}

#endif
