#if defined(LUMOS_RENDER_API_VULKAN)

#include "Platform/Vulkan/VKDevice.h"
#include "App/Application.h"
#include "iOSOS.h"
#include "iOSWindow.h"

namespace Lumos
{
	VkSurfaceKHR Graphics::VKDevice::CreatePlatformSurface(VkInstance vkInstance, Window* window)
	{
		VkSurfaceKHR surface;

        auto iosView = iOSWindow::GetIOSView();

        VkIOSSurfaceCreateInfoMVK surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
        surfaceCreateInfo.pNext = NULL;
        surfaceCreateInfo.pView = iosView;
        vkCreateIOSSurfaceMVK(vkInstance, &surfaceCreateInfo, nullptr, &surface);

		return surface;
	}

    static const char* GetPlatformSurfaceExtension()
	{
		return VK_MVK_IOS_SURFACE_EXTENSION_NAME;
	}
}

#endif
