#include "w_render_pch.h"
#include "w_graphics_device_manager.h"
#include "w_command_buffers.h"
#include <w_convert.h>

namespace wolf
{
	namespace render
	{
		namespace vulkan
		{
			class w_command_buffer_pimp
			{
			public:
				w_command_buffer_pimp() :
					_name("w_command_buffer"),
					_command_pool(0)
				{
				}

				W_RESULT load(_In_ const std::shared_ptr<w_graphics_device>& pGDevice,
					_In_ const size_t& pCount,
					_In_ const w_command_buffer_level& pLevel,
					_In_ const bool& pCreateCommandPool,
					_In_ const w_queue* pCommandPoolQueue)
				{
					const char* _trace_info = (this->_name + "::load").c_str();

					if (pCreateCommandPool)
					{
						VkCommandPoolCreateInfo _command_pool_info = {};
						_command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
						_command_pool_info.queueFamilyIndex = pCommandPoolQueue ? pCommandPoolQueue->index : pGDevice->vk_graphics_queue.index;
						_command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

						auto _hr = vkCreateCommandPool(
							pGDevice->vk_device,
							&_command_pool_info,
							nullptr,
							&this->_command_pool);
						if (_hr)
						{
							V(W_FAILED,
								w_log_type::W_ERROR,
								"creating vulkan command pool for graphics device: {}. trace info: {}",
								this->_gDevice->get_info(),
								_trace_info);

							return W_FAILED;
						}
					}

					this->_counts = pCount;

					if (!this->_counts) return W_FAILED;
					this->_gDevice = pGDevice;

					//if we have an existing command buffers
					auto _size = this->_commands.size();
					if (_size)
					{
						std::vector<VkCommandBuffer> _cmds(_size);
						for (size_t i = 0; i < _size; ++i)
						{
							_cmds[i] = std::move(_commands[i].handle);
						}
						vkFreeCommandBuffers(this->_gDevice->vk_device,
							this->_gDevice->vk_command_allocator_pool,
							static_cast<uint32_t>(_commands.size()),
							_cmds.data());
						_cmds.clear();
						this->_commands.clear();
					}


					//create the command buffer from the command pool
					VkCommandBufferAllocateInfo _command_buffer_info = {};
					_command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
					_command_buffer_info.pNext = nullptr;
					_command_buffer_info.commandPool = this->_gDevice->vk_command_allocator_pool;
					_command_buffer_info.level = (VkCommandBufferLevel)pLevel;
					_command_buffer_info.commandBufferCount = static_cast<uint32_t>(this->_counts);


					//resize them
					this->_commands.resize(this->_counts);
					std::vector<VkCommandBuffer> _cmds(this->_counts);
					auto _hr = vkAllocateCommandBuffers(this->_gDevice->vk_device,
						&_command_buffer_info,
						_cmds.data());
					if (_hr)
					{
						this->_commands.clear();
						_cmds.clear();

						V(W_FAILED,
							w_log_type::W_ERROR,
							"creating vulkan command buffers for graphics device: {}. trace info: {}",
							this->_gDevice->get_info(),
							_trace_info);

						return W_FAILED;
					}

					for (size_t i = 0; i < this->_counts; ++i)
					{
						this->_commands[i].handle = std::move(_cmds[i]);
					}
					_cmds.clear();
					return W_PASSED;
				}

				W_RESULT begin(_In_ const size_t& pCommandBufferIndex, _In_ const uint32_t pFlags)
				{
					if (pCommandBufferIndex >= this->_commands.size()) return W_FAILED;

					const char* _trace_info = (this->_name + "::begin").c_str();

					//prepare data for recording command buffers
					const VkCommandBufferBeginInfo _command_buffer_begin_info =
					{
						VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,        // Type;
						nullptr,                                            // Next
						(VkCommandBufferUsageFlags)pFlags,					// Flags
						nullptr,
					};

					auto _hr = vkBeginCommandBuffer(this->_commands.at(pCommandBufferIndex).handle, &_command_buffer_begin_info);
					if (_hr != VK_SUCCESS)
					{
						V(W_FAILED,
							w_log_type::W_ERROR,
							"begining command buffer for graphics device: {}. trace info: {}",
							this->_gDevice->get_info(),
							_trace_info);
						return W_FAILED;
					}

					return W_PASSED;
				}

				W_RESULT end(_In_ const size_t& pCommandBufferIndex)
				{
					if (pCommandBufferIndex >= this->_commands.size()) return W_FAILED;

					const char* _trace_info = (this->_name + "::end").c_str();

					auto _hr = vkEndCommandBuffer(this->_commands.at(pCommandBufferIndex).handle);
					if (_hr != VK_SUCCESS)
					{
						V(W_FAILED,
							w_log_type::W_ERROR,
							"ending command buffer for graphics device: {}. trace info: {}:",
							this->_gDevice->get_info(),
							_trace_info);
					}
					return W_PASSED;
				}

				W_RESULT flush(_In_ const size_t& pCommandBufferIndex)
				{
					if (pCommandBufferIndex >= this->_commands.size()) return W_FAILED;

					const char* _trace_info = (this->_name + "::end").c_str();

					auto _cmd = this->_commands.at(pCommandBufferIndex);

					auto _hr = vkEndCommandBuffer(_cmd.handle);
					if (_hr)
					{
						V(W_FAILED,
							w_log_type::W_ERROR,
							"ending command buffer buffer for graphics device: {}. trace info: {}",
							this->_gDevice->get_info(),
							_trace_info);

						return W_FAILED;
					}

					VkSubmitInfo _submit_info = {};
					_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
					_submit_info.commandBufferCount = 1;
					_submit_info.pCommandBuffers = &_cmd.handle;

					// Create fence to ensure that the command buffer has finished executing
					VkFenceCreateInfo _fence_create_info = {};
					_fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
					_fence_create_info.flags = 0;

					VkFence _fence;
					_hr = vkCreateFence(this->_gDevice->vk_device, &_fence_create_info, nullptr, &_fence);
					if (_hr)
					{
						V(W_FAILED,
							w_log_type::W_ERROR,
							"creating fence for command buffer for graphics device: {}. trace info: {}",
							this->_gDevice->get_info(),
							_trace_info);
						return W_FAILED;
					}

					// Submit to the queue
					_hr = vkQueueSubmit(this->_gDevice->vk_present_queue.queue,
						1,
						&_submit_info,
						_fence);
					if (_hr)
					{
						V(W_FAILED,
							w_log_type::W_ERROR,
							"submiting queue for graphics device: {}. trace info: {}",
							this->_gDevice->get_info(),
							_trace_info);
						return W_FAILED;
					}

					// Wait for the fence to signal that command buffer has finished executing
					_hr = vkWaitForFences(this->_gDevice->vk_device,
						1,
						&_fence,
						VK_TRUE,
						DEFAULT_FENCE_TIMEOUT);
					vkDestroyFence(this->_gDevice->vk_device, _fence, nullptr);

					return W_PASSED;
				}

				W_RESULT flush_all()
				{
					W_RESULT _result = W_PASSED;
					for (size_t i = 0; i < this->_commands.size(); ++i)
					{
						if (flush(i) == W_FAILED)
						{
							_result = W_FAILED;
							break;
						}
					}
					return _result;
				}

				ULONG release()
				{
					if (this->_gDevice)
					{
						std::vector<VkCommandBuffer> _cmds(this->_commands.size());
						for (size_t i = 0; i < this->_commands.size(); ++i)
						{
							_cmds[i] = std::move(this->_commands[i].handle);
						}
						vkFreeCommandBuffers(this->_gDevice->vk_device,
							this->_gDevice->vk_command_allocator_pool,
							static_cast<uint32_t>(_cmds.size()),
							_cmds.data());
						_cmds.clear();
					}

					this->_commands.clear();
					this->_counts = 0;
					this->_gDevice = nullptr;

					return 0;
				}

#pragma region Getters

				const w_command_buffer* get_commands() const
				{
					return this->_commands.data();
				}

				const size_t get_commands_size() const
				{
					return this->_commands.size();
				}

#pragma endregion 

			private:
				std::string                                         _name;
				std::shared_ptr<w_graphics_device>                  _gDevice;

				std::vector<w_command_buffer>                       _commands;
				VkCommandPool                                       _command_pool;

				size_t                                              _counts;
			};
		}
	}
}

using namespace wolf::render::vulkan;

w_command_buffers::w_command_buffers() : _pimp(new w_command_buffer_pimp())
{
    _super::set_class_name("w_command_buffer");
}

w_command_buffers::~w_command_buffers()
{
    release();
}

W_RESULT w_command_buffers::load(_In_ const std::shared_ptr<w_graphics_device>& pGDevice,
    _In_ const size_t& pCount,
    _In_ const w_command_buffer_level& pLevel,
    _In_ const bool& pCreateCommandPool,
    _In_ const w_queue* pCommandPoolQueue)
{
    if(!this->_pimp) return W_FAILED;
    
    return this->_pimp->load(pGDevice, pCount, pLevel, pCreateCommandPool, pCommandPoolQueue);
}

W_RESULT w_command_buffers::begin(_In_ const size_t& pCommandBufferIndex, _In_ const uint32_t pFlags)
{
    if(!this->_pimp) return W_FAILED;
    return this->_pimp->begin(pCommandBufferIndex, pFlags);
}

W_RESULT w_command_buffers::end(_In_ const size_t& pCommandBufferIndex)
{
    if(!this->_pimp) return W_FAILED;
    return this->_pimp->end(pCommandBufferIndex);
}

W_RESULT w_command_buffers::flush(_In_ const size_t& pCommandBufferIndex)
{
    if(!this->_pimp) return W_FAILED;
    return this->_pimp->flush(pCommandBufferIndex);
}

W_RESULT w_command_buffers::flush_all()
{
    if(!this->_pimp) return W_FAILED;
    return this->_pimp->flush_all();
}

ULONG w_command_buffers::release()
{
    if(_super::get_is_released()) return 0;
    
    SAFE_RELEASE(this->_pimp);
    
    return _super::release();
}

#pragma region Getters

const w_command_buffer* w_command_buffers::get_commands() const
{
	if (!_pimp) return 0;

	return this->_pimp->get_commands();
}

const w_command_buffer w_command_buffers::get_command_at(_In_ const size_t& pIndex) const
{
	if (!_pimp) return w_command_buffer();

	auto _size = this->_pimp->get_commands_size();
	if (pIndex >= _size) return w_command_buffer();

	auto _cmds = this->_pimp->get_commands();
	return _cmds[pIndex];
}

const size_t w_command_buffers::get_commands_size() const
{
    if (!_pimp) return 0;
    
    return this->_pimp->get_commands_size();
}

#pragma endregion

#pragma region w_indirect_draws_command_buffer

W_RESULT w_indirect_draws_command_buffer::load(_In_ const std::shared_ptr<w_graphics_device>& pGDevice, _In_ const uint32_t& pDrawCount)
{
	const std::string _trace_info = "w_indirect_draws_command_buffer::load";

	if (!this->drawing_commands.size())
	{
		V(W_FAILED,
			w_log_type::W_ERROR,
			"empty indirect drawing commands. trace info: {}",
			_trace_info);
		return W_FAILED;
	}
	if (pDrawCount > this->drawing_commands.size())
	{
		V(W_FAILED,
			w_log_type::W_ERROR,
			"draw count is greater than indirect drawing commands. trace info: {}",
			_trace_info);
		return W_FAILED;
	}

	w_buffer _staging_buffer;
	defer _(nullptr, [&](...)
	{
		_staging_buffer.release();
	});

	uint32_t _size = (uint32_t)(pDrawCount * sizeof(w_draw_indexed_indirect_command));
	//if (_staging_buffer.allocate_as_staging(pGDevice, _size) == W_FAILED)
	//{
	//	V(W_FAILED, "loading staging buffer of indirect_draw_commands", _trace_info, 3);
	//	return W_FAILED;
	//}

	if (_staging_buffer.allocate(
		pGDevice,
		_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		w_memory_usage_flag::MEMORY_USAGE_CPU_ONLY,
		false) == W_FAILED)
	{
		V(W_FAILED,
			w_log_type::W_ERROR,
			"loading staging buffer of indirect_draw_commands. trace info: {}",
			_trace_info);
		return W_FAILED;
	}

	if (_staging_buffer.bind() == W_FAILED)
	{
		V(W_FAILED,
			w_log_type::W_ERROR,
			"binding to staging buffer of indirect_draw_commands. trace info: {}",
			_trace_info);
		return W_FAILED;
	}

	if (_staging_buffer.set_data(this->drawing_commands.data()) == W_FAILED)
	{
		V(W_FAILED,
			w_log_type::W_ERROR,
			"setting data for staging buffer of indirect_draw_commands. trace info: {}",
			_trace_info);
		return W_FAILED;
	}

	//load memory for indirect buffer as seperated memory
	if (this->buffer.allocate(
		pGDevice,
		_size,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		w_memory_usage_flag::MEMORY_USAGE_GPU_ONLY,
		false) == W_FAILED)
	{
		V(W_FAILED,
			w_log_type::W_ERROR,
			"loading staging buffer of indirect_commands_buffer. trace info: {}",
			_trace_info);
		return W_FAILED;
	}

	//bind indircet buffer
	if (this->buffer.bind() == W_FAILED)
	{
		V(W_FAILED,
			w_log_type::W_ERROR,
			"binding to staging buffer of indirect_commands_buffer. trace info: {}",
			_trace_info);
		return W_FAILED;
	}

	if (_staging_buffer.copy_to(this->buffer) == W_FAILED)
	{
		V(W_FAILED,
			w_log_type::W_ERROR,
			"copy staging buffer to device buffer of indirect_commands_buffer. trace info: {}",
			_trace_info);
		return W_FAILED;
	}

	return W_PASSED;
}

#pragma endregion
