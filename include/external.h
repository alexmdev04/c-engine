#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wstatic-in-inline"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "cglm/cglm.h"
#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
#include "shaderc/shaderc.h" // TODO: statically link shaderc
// #include "vulkan/vulkan.h"
#pragma clang diagnostic pop