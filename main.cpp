#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <Blast/Gfx/GfxDevice.h>
#include <Blast/Gfx/Vulkan/VulkanDevice.h>
#include <Blast/Utility/ShaderCompiler.h>
#include <Blast/Utility/VulkanShaderCompiler.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <fstream>
#include <sstream>

#define SAFE_DELETE(x) \
    { \
        delete x; \
        x = nullptr; \
    }

#define SAFE_DELETE_ARRAY(x) \
    { \
        delete[] x; \
        x = nullptr; \
    }

blast::ShaderCompiler* g_shader_compiler = nullptr;
blast::GfxDevice* g_device = nullptr;
blast::GfxSwapChain* g_swapchain = nullptr;
blast::GfxShader* g_vert_shader = nullptr;
blast::GfxShader* g_frag_shader = nullptr;
blast::GfxPipeline* g_pipeline = nullptr;
blast::GfxBuffer* g_mesh_index_buffer = nullptr;
blast::GfxBuffer* g_mesh_vertex_buffer = nullptr;
blast::GfxBuffer* staging_buffer = nullptr;
blast::GfxTexture* g_texture = nullptr;
blast::GfxSampler* g_sampler = nullptr;

static std::string ProjectDir(PROJECT_DIR);

static std::string ReadFileData(const std::string& path);

static void RefreshSwapchain(void* window, uint32_t width, uint32_t height);

struct Vertex {
    float pos[3];
    float uv[2];
};

float vertices[] = {
        -0.5f,  -0.5f, 0.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
        -0.5f, 0.5f, 0.0f, 0.0f, 1.0f
};

unsigned int indices[] = {
        0, 1, 2, 2, 3, 0
};

int main() {
    g_shader_compiler = new blast::VulkanShaderCompiler();

    g_device = new blast::VulkanDevice();

    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(ProjectDir + "/Resource/texture.vert");
        compile_desc.stage = blast::SHADER_STAGE_VERT;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_VERT;
        shader_desc.bytecode = compile_result.bytes.data();
        shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
        g_vert_shader = g_device->CreateShader(shader_desc);
    }

    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(ProjectDir + "/Resource/texture.frag");
        compile_desc.stage = blast::SHADER_STAGE_FRAG;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_FRAG;
        shader_desc.bytecode = compile_result.bytes.data();
        shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
        g_frag_shader = g_device->CreateShader(shader_desc);
    }

    // load resource
    blast::GfxCommandBuffer* copy_cmd = g_device->RequestCommandBuffer(blast::QUEUE_COPY);
    blast::GfxBufferDesc buffer_desc;
    buffer_desc.size = sizeof(Vertex) * 4;
    buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
    buffer_desc.res_usage = blast::RESOURCE_USAGE_VERTEX_BUFFER;
    g_mesh_vertex_buffer = g_device->CreateBuffer(buffer_desc);
    {
        g_device->UpdateBuffer(copy_cmd, g_mesh_vertex_buffer, vertices, sizeof(Vertex) * 4);
        blast::GfxBufferBarrier barrier;
        barrier.buffer = g_mesh_vertex_buffer;
        barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
        g_device->SetBarrier(copy_cmd, 1, &barrier, 0, nullptr);
    }

    buffer_desc.size = sizeof(unsigned int) * 6;
    buffer_desc.mem_usage = blast::MEMORY_USAGE_CPU_TO_GPU;
    buffer_desc.res_usage = blast::RESOURCE_USAGE_INDEX_BUFFER;
    g_mesh_index_buffer = g_device->CreateBuffer(buffer_desc);
    {
        g_device->UpdateBuffer(copy_cmd, g_mesh_index_buffer, indices, sizeof(unsigned int) * 6);
        blast::GfxBufferBarrier barrier;
        barrier.buffer = g_mesh_index_buffer;
        barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
        g_device->SetBarrier(copy_cmd, 1, &barrier, 0, nullptr);
    }

    int tex_width, tex_height, tex_channels;
    std::string image_path = ProjectDir + "/Resource/test.png";
    unsigned char* pixels = stbi_load(image_path.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

    blast::GfxTextureDesc texture_desc;
    texture_desc.width = tex_width;
    texture_desc.height = tex_height;
    texture_desc.format = blast::FORMAT_R8G8B8A8_UNORM;
    texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
    texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE;
    g_texture = g_device->CreateTexture(texture_desc);
    {
        // 设置纹理为读写状态
        blast::GfxTextureBarrier barrier;
        barrier.texture = g_texture;
        barrier.new_state = blast::RESOURCE_STATE_COPY_DEST;
        g_device->SetBarrier(copy_cmd, 0, nullptr, 1, &barrier);

        // 更新纹理数据
        g_device->UpdateTexture(copy_cmd, g_texture, pixels);

        // 设置纹理为Shader可读状态
        barrier.texture = g_texture;
        barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
        g_device->SetBarrier(copy_cmd, 0, nullptr, 1, &barrier);
    }
    stbi_image_free(pixels);

    blast::GfxSamplerDesc sampler_desc = {};
    g_sampler = g_device->CreateSampler(sampler_desc);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "BlastExample", nullptr, nullptr);
    int frame_width = 0, frame_height = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        int window_width, window_height;
        glfwGetWindowSize(window, &window_width, &window_height);
        if (frame_width != window_width || frame_height != window_height) {
            frame_width = window_width;
            frame_height = window_height;
            RefreshSwapchain(glfwGetWin32Window(window), frame_width, frame_height);
        }

        blast::GfxCommandBuffer* cmd = g_device->RequestCommandBuffer(blast::QUEUE_GRAPHICS);
        g_device->RenderPassBegin(cmd, g_swapchain);

        blast::Viewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.w = frame_width;
        viewport.h = frame_height;
        g_device->BindViewports(cmd, 1, &viewport);

        blast::Rect rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = frame_width;
        rect.bottom = frame_height;
        g_device->BindScissorRects(cmd, 1, &rect);

        g_device->BindPipeline(cmd, g_pipeline);

        g_device->BindResource(cmd, g_texture, 0);

        g_device->BindSampler(cmd, g_sampler, 0);

        uint64_t vertex_offsets[] = {0};
        g_device->BindVertexBuffers(cmd, &g_mesh_vertex_buffer, 0, 1, vertex_offsets);

        g_device->BindIndexBuffer(cmd, g_mesh_index_buffer, blast::INDEX_TYPE_UINT32, 0);

        g_device->DrawIndexed(cmd, 6, 0, 0);

        g_device->RenderPassEnd(cmd);
        g_device->SubmitAllCommandBuffer();
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    g_device->DestroySampler(g_sampler);

    g_device->DestroyTexture(g_texture);

    g_device->DestroyBuffer(g_mesh_index_buffer);

    g_device->DestroyBuffer(g_mesh_vertex_buffer);

    g_device->DestroyShader(g_vert_shader);

    g_device->DestroyShader(g_frag_shader);

    g_device->DestroyPipeline(g_pipeline);

    g_device->DestroySwapChain(g_swapchain);

    SAFE_DELETE(g_device);

    SAFE_DELETE(g_shader_compiler);
    return 0;
}

void RefreshSwapchain(void* window, uint32_t width, uint32_t height) {
    blast::GfxSwapChainDesc swapchain_desc;
    swapchain_desc.window = window;
    swapchain_desc.width = width;
    swapchain_desc.height = height;
    swapchain_desc.clear_color[0] = 1.0f;
    g_swapchain = g_device->CreateSwapChain(swapchain_desc, g_swapchain);

    blast::GfxInputLayout input_layout = {};
    blast::GfxInputLayout::Element input_element;
    input_element.semantic = blast::SEMANTIC_POSITION;
    input_element.format = blast::FORMAT_R32G32B32_FLOAT;
    input_element.binding = 0;
    input_element.location = 0;
    input_element.offset = 0;
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_TEXCOORD0;
    input_element.format = blast::FORMAT_R32G32_FLOAT;
    input_element.binding = 0;
    input_element.location = 1;
    input_element.offset = offsetof(Vertex, uv);
    input_layout.elements.push_back(input_element);

    blast::GfxBlendState blend_state = {};
    blend_state.rt[0].src_factor = blast::BLEND_ONE;
    blend_state.rt[0].dst_factor = blast::BLEND_ZERO;
    blend_state.rt[0].src_factor_alpha = blast::BLEND_ONE;
    blend_state.rt[0].dst_factor_alpha = blast::BLEND_ZERO;

    blast::GfxDepthStencilState depth_stencil_state = {};
    depth_stencil_state.depth_test = true;
    depth_stencil_state.depth_write = true;

    blast::GfxRasterizerState rasterizer_state = {};
    rasterizer_state.cull_mode = blast::CULL_NONE;
    rasterizer_state.front_face = blast::FRONT_FACE_CW;
    rasterizer_state.fill_mode = blast::FILL_SOLID;

    blast::GfxPipelineDesc pipeline_desc;
    pipeline_desc.sc = g_swapchain;
    pipeline_desc.vs = g_vert_shader;
    pipeline_desc.fs = g_frag_shader;
    pipeline_desc.il = &input_layout;
    pipeline_desc.bs = &blend_state;
    pipeline_desc.rs = &rasterizer_state;
    pipeline_desc.dss = &depth_stencil_state;
    pipeline_desc.primitive_topo = blast::PRIMITIVE_TOPO_TRI_LIST;
    g_pipeline = g_device->CreatePipeline(pipeline_desc);
}

std::string ReadFileData(const std::string& path) {
    std::istream* stream = &std::cin;
    std::ifstream file;

    file.open(path, std::ios_base::binary);
    stream = &file;
    if (file.fail()) {
        BLAST_LOGW("cannot open input file %s \n", path.c_str());
        return std::string("");
    }
    return std::string((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
}