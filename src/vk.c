#include "src.c"
#define SX_VK_VERSION VK_API_VERSION_1_4
#define SX_VK_MAX_FRAMES_IN_FLIGHT 2

typedef struct {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAquiredSemaphore;
} FrameResources;

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    VmaAllocator vmaAllocator;

    u32 gfxQueueFamilyIndex;
    VkQueue gfxQueue;

    VkSwapchainKHR swapchain;
    VkImage* swapchainImages;
    u32 swapchainImageCount; //
    VkImageView* swapchainImageViews;
    u32 swapchainImageViewsCount; // 
    VkSemaphore* renderCompleteSemaphores;
    u32 renderCompleteSemaphoresCount; //
    bool requireSwapchainRecreate;
    u32 swapchainWidth;
    u32 swapchainHeight;

    VkImage depthImage;
    VkImageView depthImageView;
    VmaAllocation depthImageAllocation;

    VkFormat swapchainImageFormat;
    VkFormat depthFormat;

    VkShaderModule vertShader;
    VkShaderModule fragShader;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkSemaphore timelineSemaphore;

    FrameResources frameResources[SX_VK_MAX_FRAMES_IN_FLIGHT];
    u32 frameIndex;
    u64 nextSignalValue;
} Vk;    

VKAPI_ATTR VkBool32 VKAPI_CALL SX_Vk_DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
) {
    SDL_Log("[VK Validation Layer] %s", pCallbackData->pMessage);
    return VK_FALSE;
}

bool SX_Vk_TryCreateInstance(Vk* vk) {
    if (volkInitialize() != VK_SUCCESS) {
        return false;
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Swade",
        .apiVersion = SX_VK_VERSION
    };

    u32 sdlExtCount = 0;
    c_str_arr instExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);

    // adds extensions
    const u32 extCount = sdlExtCount + 1;
    c_str exts[extCount];
    memcpy(exts, instExts, sizeof(c_str) * sdlExtCount);
    exts[extCount - 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    
    const u32 layerCount = 1;
    c_str layers[layerCount];
    layers[0] = "VK_LAYER_KHRONOS_validation";
    
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = 
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = 
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &SX_Vk_DebugCallback
    };
    
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debugInfo,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = extCount,
        .ppEnabledExtensionNames = exts
    };
    
    if (vkCreateInstance(&createInfo, nullptr, &vk->instance) != VK_SUCCESS) {
        return false;
    }

    volkLoadInstance(vk->instance);
    return true;
}

bool SX_Vk_TryCreateSurface(Vk* vk, SDL_Window* window) {
    if (!SDL_Vulkan_CreateSurface(
            window, vk->instance, nullptr, &vk->surface
        )) {
        return false;
    }

    return true;
}

bool SX_Vk_TryGetPhysicalDevice(Vk* vk) {
    u32 physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(vk->instance, &physicalDeviceCount, nullptr);
    VkPhysicalDevice physicalDevices[physicalDeviceCount];
    vkEnumeratePhysicalDevices(
        vk->instance, &physicalDeviceCount, physicalDevices
    );

    if (!physicalDeviceCount) {
        return false;
    }
    
    vk->physicalDevice = physicalDevices[0];
    for (u32 i = 0; i < physicalDeviceCount; i++) {
        VkPhysicalDevice physicalDevice = physicalDevices[i];
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            vk->physicalDevice = physicalDevice;
            break;
        }
    }

    u32 surfaceFormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physicalDevice, vk->surface, &surfaceFormatCount, nullptr);
    VkSurfaceFormatKHR surfaceFormats[surfaceFormatCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physicalDevice, vk->surface, &surfaceFormatCount, surfaceFormats);

    bool formatSupported = false;
    for (u32 i = 0; i < surfaceFormatCount; i++) {
        VkSurfaceFormatKHR surfaceFormat = surfaceFormats[i];
        if (surfaceFormat.format == vk->swapchainImageFormat) {
            formatSupported = true;
            break;
        }
    }

    if (!formatSupported) {
        SX_Log_Now("[SX_Vk] Unsupported swapchain format.\n");
        return false;
    }

    return true;
}

bool SX_Vk_TryFindGraphicsQueue(Vk* vk) {
    u32 count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties2(
        vk->physicalDevice, &count, nullptr
    );

    VkQueueFamilyProperties2 queueFamilyProperties[count];
    for (u32 i = 0; i < count; i++) {
        queueFamilyProperties[i] = (VkQueueFamilyProperties2){
            .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2
        };
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(
        vk->physicalDevice, &count, queueFamilyProperties
    );

    for (u32 i = 0; i < count; i++) {
        VkQueueFamilyProperties2 props = queueFamilyProperties[i];

        VkBool32 hasPresentationSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            vk->physicalDevice, i, vk->surface, &hasPresentationSupport
        );

        if (hasPresentationSupport &&
            props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vk->gfxQueueFamilyIndex = i;
            return true;
        }
    }

    return false;
}

bool SX_Vk_TryCreateDevice(Vk* vk) {
    VkPhysicalDeviceVulkan14Features queryFeats1_4 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = nullptr
    };

    VkPhysicalDeviceVulkan13Features queryFeats1_3 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &queryFeats1_4
    };

    VkPhysicalDeviceVulkan12Features queryFeats1_2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &queryFeats1_3
    };

    VkPhysicalDeviceFeatures2 queryFeats = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &queryFeats1_2
    };
    
    vkGetPhysicalDeviceFeatures2(vk->physicalDevice, &queryFeats);
    
    if (!queryFeats1_3.dynamicRendering ||
        !queryFeats1_3.synchronization2 ||
        !queryFeats1_2.timelineSemaphore
    ) {
        SX_Log_Now("[SX_Vk] Unsupported GPU (missing features).\n");
        return false;
    }
    
    VkPhysicalDeviceVulkan14Features feats14 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = nullptr
    };
    
    VkPhysicalDeviceVulkan13Features feats13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &feats14,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };
    
    VkPhysicalDeviceVulkan12Features feats12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &feats13,
        .timelineSemaphore = VK_TRUE
    };
    
    VkPhysicalDeviceFeatures2 feats = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &feats12
    };
    
    f32 queuePriorties[] = { 1.0f };
    VkDeviceQueueCreateInfo gfxQueueInfo = { 
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk->gfxQueueFamilyIndex,
        .queueCount = countof(queuePriorties),
        .pQueuePriorities = queuePriorties
    };
    
    c_str deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }; 
    
    VkDeviceCreateInfo deviceCreateInfo = { 
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &feats,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &gfxQueueInfo,
        .enabledExtensionCount = countof(deviceExtensions),
        .ppEnabledExtensionNames = deviceExtensions,
        .pEnabledFeatures = nullptr
    };
    
    if (vkCreateDevice(vk->physicalDevice, &deviceCreateInfo, nullptr, &vk->device) != VK_SUCCESS) {
        return false;
    }
    
    vkGetDeviceQueue(vk->device, vk->gfxQueueFamilyIndex, 0, &vk->gfxQueue);
    if (!vk->gfxQueue) {
        SX_Log_Now("[SX_Vk] Failed to get graphics queue.\n");
        return false;
    }

    return true;
}

bool SX_Vk_TryInitVma(Vk* vk) {
    VmaVulkanFunctions vmaFuncInfo = { };
    VmaAllocatorCreateInfo vmaAllocCreateInfo = { 
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = vk->physicalDevice,
        .device = vk->device,
        .pVulkanFunctions = &vmaFuncInfo,
        .instance = vk->instance,
        .vulkanApiVersion = SX_VK_VERSION
    };
    
    vmaImportVulkanFunctionsFromVolk(&vmaAllocCreateInfo, &vmaFuncInfo);

    if (vmaCreateAllocator(&vmaAllocCreateInfo, &vk->vmaAllocator) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool SX_Vk_TryCreateSwapchain(Vk* vk, i32 width, i32 height) {
    vk->swapchainWidth = width;
    vk->swapchainHeight = height;

    VkSurfaceCapabilitiesKHR surfaceCapabilities = { };
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            vk->physicalDevice, vk->surface, &surfaceCapabilities) 
        != VK_SUCCESS) {

        SX_Log_Now("[SX_Vk] Failed to get surface capabilities.\n");
    }

    // u32 requestedImageCount = SDL_max(2U, surfaceCapabilities.minImageCount);
    // if (surfaceCapabilities.maxImageCount > 0) {
    //     requestedImageCount = SDL_min(requestedImageCount, surfaceCapabilities.maxImageCount);
    // }

    VkSwapchainCreateInfoKHR swapchainCreateInfo = { 
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk->surface,
        // .minImageCount = requestedImageCount,
        .minImageCount = surfaceCapabilities.minImageCount,
        .imageFormat = vk->swapchainImageFormat,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = { vk->swapchainWidth, vk->swapchainHeight },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        // .preTransform = surfaceCapabilities.currentTransform,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        // .presentMode = VK_PRESENT_MODE_FIFO_KHR // VSYNC ON
        .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR // VSYNC OFF
    };

    if (vkCreateSwapchainKHR(vk->device, &swapchainCreateInfo, nullptr, &vk->swapchain) != VK_SUCCESS) {
        SX_Log_Now("[SX_Vk] Failed to create swapchain.\n");
        return false;
    }
    
    // arrays might need to be first time malloced otherwise realloced
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &imageCount, nullptr);
    vk->swapchainImages = malloc(sizeof(VkImage) * imageCount);
    vk->swapchainImageCount = imageCount;

    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &imageCount, vk->swapchainImages);
    vk->swapchainImageViews = malloc(sizeof(VkImageView) * imageCount);
    vk->swapchainImageViewsCount = imageCount;

    vk->renderCompleteSemaphores = malloc(sizeof(VkSemaphore) * imageCount);
    vk->renderCompleteSemaphoresCount = imageCount;

    for (u32 i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo imageViewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vk->swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vk->swapchainImageFormat,
            .subresourceRange = { 
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                // .baseMipLevel = 0,
                .levelCount = 1,
                // .baseArrayLayer = 0,
                // .layerCount = 0
                .layerCount = 1
            }
        };

        if (vkCreateImageView(vk->device, &imageViewInfo, nullptr, &vk->swapchainImageViews[i]) != VK_SUCCESS) {
            SX_Log_Now("[SX_Vk] Failed to create swapchain image view.\n");
            return false;
        }

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        
        if (vkCreateSemaphore(vk->device, & semaphoreCreateInfo, nullptr, &vk->renderCompleteSemaphores[i]) != VK_SUCCESS) {
            SX_Log_Now("[SX_Vk] Failed to create swapchain semaphore.\n");
            return false;
        }
    }

    // vk->renderCompleteSemaphores = malloc(sizeof(VkSemaphore) * imageCount);
    // for (u32 i = 0; i < imageCount; i++) {
    //     VkSemaphoreCreateInfo semaphoreCreateInfo = {
    //         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    //     };
        
    //     if (vkCreateSemaphore(vk->device, & semaphoreCreateInfo, nullptr, &vk->renderCompleteSemaphores[i]) != VK_SUCCESS) {
    //         SX_Log_Now("[SX_Vk] Failed to create swapchain semaphore.\n");
    //         return false;
    //     }
    // }

    VkImageCreateInfo depthCreateInfo = { 
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = vk->depthFormat,
        .extent = { 
            .width = vk->swapchainWidth,
            .height = vk->swapchainHeight,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocInfo = {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    if (vmaCreateImage(vk->vmaAllocator, &depthCreateInfo, &allocInfo, &vk->depthImage, &vk->depthImageAllocation, nullptr) != VK_SUCCESS) {
        SX_Log_Now("[SX_Vk] Failed to create depth image.\n");
        return false;
    }
    
    VkImageViewCreateInfo depthImageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vk->depthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = vk->depthFormat,
        .subresourceRange = { 
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1
        }
    };

    if (vkCreateImageView(vk->device, &depthImageViewCreateInfo, nullptr, &vk->depthImageView) != VK_SUCCESS){
        SX_Log_Now("[SX_Vk] Failed to create depth image view.\n");
        return false;
    }

    return true;
}

void SX_Vk_DestroySwapchain(Vk* vk) {
    // TODO: could optimize by only reallocing instead of always mallocing and freeing

    for (u32 i = 0; i < vk->swapchainImageViewsCount; i++) {
        vkDestroyImageView(vk->device, vk->swapchainImageViews[i], nullptr);
    }
    free(vk->swapchainImageViews);
    
    for (u32 i = 0; i < vk->renderCompleteSemaphoresCount; i++) {
        vkDestroySemaphore(vk->device, vk->renderCompleteSemaphores[i], nullptr);
    }
    free(vk->renderCompleteSemaphores);

    if (vk->swapchain) {
        vkDestroySwapchainKHR(vk->device, vk->swapchain, nullptr);
        vk->swapchain = nullptr;
    }

    if (vk->depthImageView) {
        vkDestroyImageView(vk->device, vk->depthImageView, nullptr);
        vmaDestroyImage(vk->vmaAllocator, vk->depthImage, vk->depthImageAllocation);
        vk->depthImageView = nullptr;
    }
}

// bool SX_ConcatValidFileNameToPath(c_str path, c_str fileName, u32 maxFileNameLength, c_str* out) {
//     const u32 pathLen = strlen(path);
//     const u32 fileNameLen = strnlen(fileName, maxFileNameLength);
//     if (fileNameLen == maxFileNameLength && fileName[maxFileNameLength - 1] != '\0') {
//         // SX_Log_Now("[SX_Vk] Invalid c_str, file name max length is %i.\n", maxFileNameLength);
//         return false;
//     }

//     char shaderPath[shaderDirLen + fileNameLen];
//     *out = ;
//     strcpy(shaderPath, shaderDir);
//     strcat(shaderPath, fileName);
// }

VkShaderModule SX_Vk_CreateShaderModule(Vk* vk, c_str fileName, shaderc_shader_kind kind) { 
    const u32 maxFileNameLen = 24;
    c_str shaderDir = "shaders/";
    const u32 shaderDirLen = strlen(shaderDir);
    const u32 fileNameLen = strnlen(fileName, maxFileNameLen + 1);
    if ((fileNameLen == maxFileNameLen + 1) && (fileName[maxFileNameLen] != '\0')) {
        SX_Log_Now("[SX_Vk] Invalid c_str, shader name max length is %i.\n", maxFileNameLen);
        return nullptr;
    }

    char shaderPath[shaderDirLen + fileNameLen];
    strcpy(shaderPath, shaderDir);
    strcat(shaderPath, fileName);

    uptr shaderFileSize = 0;
    void* shaderFile = SDL_LoadFile(shaderPath, &shaderFileSize);

    if (shaderFile == nullptr) {
        SX_Log_Now("[SX_Vk] Failed to load shader file %s.\n", shaderPath);
        return nullptr;
    }

    SDL_Log("[SX_Vk] Compiling shader %s...", shaderPath);

    auto compiler = shaderc_compiler_initialize();
    auto compileOptions = shaderc_compile_options_initialize();
    
    shaderc_compile_options_set_target_env(compileOptions, shaderc_target_env_vulkan, SX_VK_VERSION);
    shaderc_compile_options_set_target_spirv(compileOptions, shaderc_spirv_version_1_6);
    shaderc_compile_options_set_optimization_level(compileOptions, shaderc_optimization_level_performance);

    auto compilationResult = shaderc_compile_into_spv(
        compiler,
        shaderFile,
        shaderFileSize,
        kind,
        fileName,
        "main",
        compileOptions
    );

    auto status = shaderc_result_get_compilation_status(compilationResult);

    if (status != shaderc_compilation_status_success) {
        SX_Log_Now(
            "[SX_Vk] Shader compilation failed for '%s': %s\n",
            shaderPath,
            shaderc_result_get_error_message(compilationResult)
        );
        return nullptr;
    }

    
    auto shader = (const u32*)shaderc_result_get_bytes(compilationResult);
    uptr shaderLen = shaderc_result_get_length(compilationResult);

    VkShaderModuleCreateInfo moduleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderLen,
        .pCode = shader
    };

    VkShaderModule shaderModule = nullptr;
    if (vkCreateShaderModule(vk->device, &moduleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return nullptr;
    }

    SDL_free(shaderFile);

    return shaderModule;
}

bool SX_Vk_TryCreateShaders(Vk* vk) {
    vk->vertShader = SX_Vk_CreateShaderModule(vk, "shader.vert", shaderc_vertex_shader);
    if (!vk->vertShader) {
        SX_Log_Now("[SX_Vk] Failed to create vertex shader module.\n");
        return false;
    }

    vk->fragShader = SX_Vk_CreateShaderModule(vk, "shader.frag", shaderc_fragment_shader);
    if (!vk->fragShader) {
        SX_Log_Now("[SX_Vk] Failed to create fragment shader module.\n");
        return false;
    }

    return true;
}

VkPipeline SX_Vk_CreateGraphicsPipeline(Vk* vk) {
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0
    };

    if (vkCreatePipelineLayout(vk->device, & pipelineLayoutCreateInfo, nullptr, &vk->pipelineLayout) != VK_SUCCESS) {
        SX_Log_Now("[SX_Vk] Failed to create pipeline layout.\n");
        return nullptr;
    }

    c_str entryPoint = "main";
    VkPipelineShaderStageCreateInfo shaderStages[] = { 
        {
           .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
           .stage = VK_SHADER_STAGE_VERTEX_BIT,
           .module = vk->vertShader,
           .pName = entryPoint
        }, {
           .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
           .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
           .module = vk->fragShader,
           .pName = entryPoint
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = { 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .stencilTestEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };

    VkPipelineRasterizationStateCreateInfo rasterStateCreateInfo = { 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState blendAttachmentState = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_FLAG_BITS_MAX_ENUM
    };

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachmentState
    };

    VkDynamicState dynamicState[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = countof(dynamicState),
        .pDynamicStates = dynamicState
    };

    VkPipelineRenderingCreateInfo renderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &vk->swapchainImageFormat,
        .depthAttachmentFormat = vk->depthFormat
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCreateInfo,
        .stageCount = countof(shaderStages),
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputStateCreateInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pViewportState = &viewportStateCreateInfo,
        .pRasterizationState = &rasterStateCreateInfo,
        .pMultisampleState = &multisampleStateCreateInfo,
        .pDepthStencilState = &depthStencilCreateInfo,
        .pColorBlendState = &colorBlendStateCreateInfo,
        .pDynamicState = &dynamicStateCreateInfo,
        .layout = vk->pipelineLayout,
        .renderPass = VK_NULL_HANDLE
    };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(vk->device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
        SX_Log_Now("[SX_Vk] Failed to create pipeline.\n");
        return nullptr;
    }
    
    return pipeline;
}

bool SX_Vk_TryCreateGraphicsPipelines(Vk* vk) {
    vk->pipeline = SX_Vk_CreateGraphicsPipeline(vk);
    if (!vk->pipeline) {
        return false;
    }
    return true;
}

bool SX_Vk_TryCreateSyncResources(Vk* vk) {
    VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = SX_VK_MAX_FRAMES_IN_FLIGHT
    };

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphoreTypeCreateInfo
    };

    if (vkCreateSemaphore(vk->device, &semaphoreCreateInfo, nullptr, &vk->timelineSemaphore) != VK_SUCCESS) {
        SX_Log_Now("[SX_Vk] Failed to create semaphore.\n");
        return false;
    }

    for (u32 i = 0; i < countof(vk->frameResources); i++) {
        FrameResources* frameResource = &vk->frameResources[i];
        
        VkSemaphoreCreateInfo semaphoreCreateInfo = { 
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        
        if (vkCreateSemaphore(vk->device, &semaphoreCreateInfo, nullptr, &frameResource->imageAquiredSemaphore) != VK_SUCCESS) {
            SX_Log_Now("[SX_Vk] Failed to create the per-frame image-aquired semaphore.\n");
            return false;
        }
    }

    return true;
}

bool SX_Vk_TryCreateCommandBuffers(Vk* vk) {
    for (u32 i = 0; i < countof(vk->frameResources); i++) {
        FrameResources* frameResource = &vk->frameResources[i];

        VkCommandPoolCreateInfo commandPoolCreateInfo = { 
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = vk->gfxQueueFamilyIndex
        };

        if (vkCreateCommandPool(vk->device, &commandPoolCreateInfo, nullptr, &frameResource->commandPool) != VK_SUCCESS) {
            SX_Log_Now("[SX_Vk] Failed to create command pool.\n");
            return false;
        }
        
        VkCommandBufferAllocateInfo commandBufferAllocateInfo = { 
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frameResource->commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        if (vkAllocateCommandBuffers(vk->device, &commandBufferAllocateInfo, &frameResource->commandBuffer) != VK_SUCCESS) {
            SX_Log_Now("[SX_Vk] Failed to create command buffer.\n");
            return false;
        }
    }
    return true;
}

bool SX_Vk_TryInit(Vk* vk, SDL_Window* window) {
    vk->swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    vk->depthFormat = VK_FORMAT_D32_SFLOAT;
    vk->nextSignalValue = SX_VK_MAX_FRAMES_IN_FLIGHT + 1;

    SDL_Log("[SX_Vk] Creating vulkan instance...");

    if (!SX_Vk_TryCreateInstance(vk)) {
        SX_Log_Now("[SX_Vk] Instance creation failed.\n");
        return false;
    }

    SDL_Log("[SX_Vk] Creating surface...");

    if (!SX_Vk_TryCreateSurface(vk, window)) {
        SX_Log_Now("[SX_Vk] Surface creation failed.\n");
        return false;
    }
    
    SDL_Log("[SX_Vk] Getting physical device...");

    if (!SX_Vk_TryGetPhysicalDevice(vk)) {
        SX_Log_Now("[SX_Vk] No GPU found.\n");
        return false;
    }

    SDL_Log("[SX_Vk] Finding graphics queue...");

    if (!SX_Vk_TryFindGraphicsQueue(vk)) {
        SX_Log_Now("[SX_Vk] Unsupported GPU (queue families).\n");
        return false;
    }
    
    SDL_Log("[SX_Vk] Creating logical vulkan device...");

    if (!SX_Vk_TryCreateDevice(vk)) {
        SX_Log_Now("[SX_Vk] Failed to create logical device.\n");
        return false;
    }

    SDL_Log("[SX_Vk] Initializing VMA...");
    
    if (!SX_Vk_TryInitVma(vk)) {
        SX_Log_Now("[SX_Vk] VMA Initialization failed.\n");
        return false;        
    }
    
    SDL_Log("[SX_Vk] Creating swapchain...");
    
    SX_GetWindowSize(window);
    
    if (!SX_Vk_TryCreateSwapchain(vk, windowW, windowH)) {
        SX_Log_Now("[SX_Vk] Swapchain creation failed.\n");
        return false;
    }
    
    SDL_Log("[SX_Vk] Creating shaders...");
    
    if (!SX_Vk_TryCreateShaders(vk)) {
        SX_Log_Now("[SX_Vk] Failed to create shaders.\n");
        return false;
    }

    SDL_Log("[SX_Vk] Creating graphics pipelines...");
    
    if (!SX_Vk_TryCreateGraphicsPipelines(vk)) {
        SX_Log_Now("[SX_Vk] Failed to create graphics pipelines.\n");
        return false;
    }

    SDL_Log("[SX_Vk] Creating sync resources...");
    
    if (!SX_Vk_TryCreateSyncResources(vk)) {
        SX_Log_Now("[SX_Vk] Failed to create sync resources.\n");
        return false;
    }

    SDL_Log("[SX_Vk] Creating command buffers...");
    
    if (!SX_Vk_TryCreateCommandBuffers(vk)) {
        SX_Log_Now("[SX_Vk] Failed to create command buffers.\n");
        return false;
    }
    
    SDL_Log("[SX_Vk] Absolute Vulkan.");
    return true;
}

void SX_Vk_Stop(Vk* vk) {
    vkDeviceWaitIdle(vk->device);

    if (vk->timelineSemaphore) {
        vkDestroySemaphore(vk->device, vk->timelineSemaphore, nullptr);
    }
    
    for (u32 i = 0; i < countof(vk->frameResources); i++) {
        FrameResources* frameResource = &vk->frameResources[i];
        vkDestroySemaphore(vk->device, frameResource->imageAquiredSemaphore, nullptr);
        vkDestroyCommandPool(vk->device, frameResource->commandPool, nullptr);
    }

    if (vk->pipelineLayout) {
        vkDestroyPipelineLayout(vk->device, vk->pipelineLayout, nullptr);
    }

    if (vk->pipeline) {
        vkDestroyPipeline(vk->device, vk->pipeline, nullptr);
    }

    if (vk->vertShader) {
        vkDestroyShaderModule(vk->device, vk->vertShader, nullptr);
    }
    
    if (vk->fragShader) {
        vkDestroyShaderModule(vk->device, vk->fragShader, nullptr);
    }

    SX_Vk_DestroySwapchain(vk);

    if (vk->vmaAllocator) {
        vmaDestroyAllocator(vk->vmaAllocator);
    }

    if (vk->surface) {
        vkDestroySurfaceKHR(vk->instance, vk->surface, nullptr);
    }

    if (vk->device) {
        vkDestroyDevice(vk->device, nullptr);
    }

    if (vk->instance) {
        vkDestroyInstance(vk->instance, nullptr);
    }

    if (vk->swapchainImages) {
        free(vk->swapchainImages);
    }
    
    if (vk->swapchainImageViews) {
        free(vk->swapchainImageViews);
    }

    volkFinalize();
}

void SX_Vk_Render(Vk* vk, SDL_Window* window) {
    if (vk->requireSwapchainRecreate) {
        vkDeviceWaitIdle(vk->device);
        SX_Vk_DestroySwapchain(vk);
        SX_GetWindowSize(window);
        SX_Vk_TryCreateSwapchain(vk, windowW, windowH);
        vk->requireSwapchainRecreate = false;
    }

    const u32 frameResourceIndex = vk->frameIndex++ % SX_VK_MAX_FRAMES_IN_FLIGHT;
    const u64 signalValue = vk->nextSignalValue++;
    const u64 waitValue = signalValue - SX_VK_MAX_FRAMES_IN_FLIGHT;

    VkSemaphoreWaitInfo semaphoreWaitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &vk->timelineSemaphore,
        .pValues = &waitValue
    };

    vkWaitSemaphores(vk->device, &semaphoreWaitInfo, UINT64_MAX);

    FrameResources frameResource = vk->frameResources[frameResourceIndex];
    vkResetCommandPool(vk->device, frameResource.commandPool, 0);

    u32 imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        vk->device,
        vk->swapchain,
        UINT64_MAX,
        frameResource.imageAquiredSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        vk->requireSwapchainRecreate = true;
        return;
    } else if (acquireResult == VK_SUBOPTIMAL_KHR) {
        vk->requireSwapchainRecreate = true;
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    vkBeginCommandBuffer(frameResource.commandBuffer, &commandBufferBeginInfo);

    VkImageMemoryBarrier2 layoutBarriers[] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = vk->swapchainImages[imageIndex],
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }, {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .image = vk->depthImage,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    };

    VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = countof(layoutBarriers),
        .pImageMemoryBarriers = layoutBarriers
    };

    vkCmdPipelineBarrier2(frameResource.commandBuffer, &dependencyInfo);

    VkRenderingAttachmentInfo renderingAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = vk->swapchainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = { .float32 = { 0.01f, 0.01f, 0.01f, 1.0f } } }
    };
    
    VkRenderingAttachmentInfo depthAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = vk->depthImageView,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = { .depthStencil = { 1.0f, 0.0f } }
    };

    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = { vk->swapchainWidth, vk->swapchainHeight }
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &renderingAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo
    };

    vkCmdBeginRendering(frameResource.commandBuffer, &renderingInfo);
    {
        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = (f32)vk->swapchainWidth,
            .height = (f32)vk->swapchainHeight
        };

        vkCmdSetViewport(frameResource.commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {
            .offset = { 0, 0 },
            .extent = { vk->swapchainWidth, vk->swapchainHeight }
        };

        vkCmdSetScissor(frameResource.commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(frameResource.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipeline);
        vkCmdDraw(frameResource.commandBuffer, 3, 1, 0, 0);
    }

    vkCmdEndRendering(frameResource.commandBuffer);

    VkImageMemoryBarrier2 presentLayoutBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = vk->swapchainImages[imageIndex],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo presentDependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &presentLayoutBarrier
    };

    vkCmdPipelineBarrier2(frameResource.commandBuffer, &presentDependencyInfo);

    vkEndCommandBuffer(frameResource.commandBuffer);

    VkSemaphoreSubmitInfo imageAcquireWaitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = frameResource.imageAquiredSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSemaphoreSubmitInfo semaphoreSignals[] = {
        { 
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk->renderCompleteSemaphores[imageIndex],
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        }, { 
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk->timelineSemaphore,
            .value = signalValue,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        }
    };

    VkCommandBufferSubmitInfo commandBufferSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = frameResource.commandBuffer
    };

    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &imageAcquireWaitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandBufferSubmitInfo,
        .signalSemaphoreInfoCount = countof(semaphoreSignals),
        .pSignalSemaphoreInfos = semaphoreSignals
    };

    vkQueueSubmit2(vk->gfxQueue, 1, &submitInfo, VK_NULL_HANDLE);

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk->renderCompleteSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &vk->swapchain,
        .pImageIndices = &imageIndex,
        .pResults = nullptr
    };

    vkQueuePresentKHR(vk->gfxQueue, &presentInfo);
}
