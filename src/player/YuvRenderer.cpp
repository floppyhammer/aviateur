﻿#include "YuvRenderer.h"
#include "libavutil/pixfmt.h"

auto vertCode = R"(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

out vec2 v_texCoord;

void main() {
    gl_Position = vec4(aPos, 1.0f);
    v_texCoord = aUV;
}
)";

auto fragCode =
    R"(
in vec2 v_texCoord;

uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;

layout(std140) uniform bUniform0 {
    int pixFmt;
    int pad0;
    int pad1;
    int pad2;
};

void main() {
    vec3 yuv;
    vec3 rgb;
    if (pixFmt == 0 || pixFmt == 12) {
        //yuv420p
        yuv.x = texture2D(tex_y, v_texCoord).r;
        yuv.y = texture2D(tex_u, v_texCoord).r - 0.5;
        yuv.z = texture2D(tex_v, v_texCoord).r - 0.5;
        rgb = mat3( 1.0,       1.0,         1.0,
                    0.0,       -0.3455,  1.779,
                    1.4075, -0.7169,  0.0) * yuv;
    } else if( pixFmt == 23 ){
        // NV12
        yuv.x = texture2D(tex_y, v_texCoord).r;
        yuv.y = texture2D(tex_u, v_texCoord).r - 0.5;
        yuv.z = texture2D(tex_u, v_texCoord).a - 0.5;
        rgb = mat3( 1.0,       1.0,         1.0,
                    0.0,       -0.3455,  1.779,
                    1.4075, -0.7169,  0.0) * yuv;

    } else {
        //YUV444P
        yuv.x = texture2D(tex_y, v_texCoord).r;
        yuv.y = texture2D(tex_u, v_texCoord).r - 0.5;
        yuv.z = texture2D(tex_v, v_texCoord).r - 0.5;

        rgb.x = clamp( yuv.x + 1.402 *yuv.z, 0.0, 1.0);
        rgb.y = clamp( yuv.x - 0.34414 * yuv.y - 0.71414 * yuv.z, 0.0, 1.0);
        rgb.z = clamp( yuv.x + 1.772 * yuv.y, 0.0, 1.0);
    }
    gl_FragColor = vec4(rgb, 1.0);
}
)";

struct FragUniformBlock {
    int pixFmt;
    int pad0;
    int pad1;
    int pad2;
};

YuvRenderer::YuvRenderer(std::shared_ptr<Pathfinder::Device> device, std::shared_ptr<Pathfinder::Queue> queue) {
    mDevice = device;
    mQueue = queue;
}

void YuvRenderer::init() {
    mRenderPass = mDevice->create_render_pass(
        Pathfinder::TextureFormat::Rgba8Unorm, Pathfinder::AttachmentLoadOp::Clear, "yuv render pass");

    initPipeline();
    initGeometry();
}

void YuvRenderer::resize(int width, int height) {
    if (m_itemWidth == width && m_itemHeight == height) {
        return;
    }
    m_itemWidth = width;
    m_itemHeight = height;
}

void YuvRenderer::initGeometry() {
    // Set up vertex data (and buffer(s)) and configure vertex attributes.
    float vertices[] = {
        // Positions, UVs.
        -1.0, -1.0, 1.0, 0.0, 0.0, // 0
        1.0,  -1.0, 1.0, 1.0, 0.0, // 1
        1.0,  1.0,  1.0, 1.0, 1.0, // 2
        -1.0, -1.0, 1.0, 0.0, 0.0, // 3
        1.0,  1.0,  1.0, 1.0, 1.0, // 4
        -1.0, 1.0,  1.0, 0.0, 1.0 // 5
    };

    mVertexBuffer = mDevice->create_buffer(
        { Pathfinder::BufferType::Vertex, sizeof(vertices), Pathfinder::MemoryProperty::DeviceLocal },
        "yuv renderer vertex buffer");

    mSampler = mDevice->create_sampler(Pathfinder::SamplerDescriptor {});

    auto encoder = mDevice->create_command_encoder("upload yuv vertex buffer");
    encoder->write_buffer(mVertexBuffer, 0, sizeof(vertices), vertices);
    mQueue->submit_and_wait(encoder);
}

void YuvRenderer::initPipeline() {
    const auto vert_source = std::vector<char>(vertCode, vertCode + sizeof(vertCode));
    const auto frag_source = std::vector<char>(fragCode, fragCode + sizeof(fragCode));

    std::vector<Pathfinder::VertexInputAttributeDescription> attribute_descriptions;

    uint32_t stride = 5 * sizeof(float);

    attribute_descriptions.push_back(
        { 0, 3, Pathfinder::DataType::f32, stride, 0, Pathfinder::VertexInputRate::Vertex });

    attribute_descriptions.push_back(
        { 0, 2, Pathfinder::DataType::f32, stride, 3 * sizeof(float), Pathfinder::VertexInputRate::Vertex });

    auto blend_state = Pathfinder::BlendState::from_over();

    mUniformBuffer = mDevice->create_buffer(
        { Pathfinder::BufferType::Uniform, sizeof(FragUniformBlock),
          Pathfinder::MemoryProperty::HostVisibleAndCoherent },
        "yuv renderer uniform buffer");

    mDescriptorSet = mDevice->create_descriptor_set();
    mDescriptorSet->add_or_update(
        {
            Pathfinder::Descriptor::uniform(0, Pathfinder::ShaderStage::Fragment, "bUniform0", mUniformBuffer),
            Pathfinder::Descriptor::sampled(1, Pathfinder::ShaderStage::Fragment, "tex_y"),
            Pathfinder::Descriptor::sampled(2, Pathfinder::ShaderStage::Fragment, "tex_u"),
            Pathfinder::Descriptor::sampled(3, Pathfinder::ShaderStage::Fragment, "tex_v"),
        });

    mPipeline = mDevice->create_render_pipeline(
        mDevice->create_shader_module(vert_source, Pathfinder::ShaderStage::Vertex, "yuv vert"),
        mDevice->create_shader_module(frag_source, Pathfinder::ShaderStage::Fragment, "yuv frag"),
        attribute_descriptions, blend_state, mDescriptorSet, Pathfinder::TextureFormat::Rgba8Unorm, "yuv pipeline");
}

void YuvRenderer::updateTextureInfo(int width, int height, int format) {
    if (width == 0 || height == 0) {
        return;
    }

    mPixFmt = format;

    mTexY = mDevice->create_texture({ { width, height }, Pathfinder::TextureFormat::R8 }, "y texture");
    // //    mTexY->setFixedSamplePositions(false);
    // mTexY->setMinificationFilter(QOpenGLTexture::Nearest);
    // mTexY->setMagnificationFilter(QOpenGLTexture::Nearest);
    // mTexY->setWrapMode(QOpenGLTexture::ClampToEdge);

    if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P) {
        mTexU = mDevice->create_texture({ { width / 2, height / 2 }, Pathfinder::TextureFormat::R8 }, "u texture");

        mTexV = mDevice->create_texture({ { width / 2, height / 2 }, Pathfinder::TextureFormat::R8 }, "v texture");
    } else if (format == AV_PIX_FMT_NV12) {
        mTexU = mDevice->create_texture({ { width / 2, height / 2 }, Pathfinder::TextureFormat::Rg8 }, "u texture");

        // V is not used for NV12.
        mTexV = mDummyTex;
    }
    //  yuv444p
    else {
        mTexU = mDevice->create_texture({ { width, height }, Pathfinder::TextureFormat::R8 }, "u texture");

        mTexV = mDevice->create_texture({ { width, height }, Pathfinder::TextureFormat::R8 }, "v texture");
    }
    mTextureAllocated = true;
}

void YuvRenderer::updateTextureData(const std::shared_ptr<AVFrame> &data) {
    float frameWidth = m_itemWidth;
    float frameHeight = m_itemHeight;
    if (m_itemWidth * (1.0 * data->height / data->width) < m_itemHeight) {
        frameHeight = frameWidth * (1.0 * data->height / data->width);
    } else {
        frameWidth = frameHeight * (1.0 * data->width / data->height);
    }
    float x = (m_itemWidth - frameWidth) / 2;
    float y = (m_itemHeight - frameHeight) / 2;
    // GL顶点坐标转换
    float x1 = -1 + 2.0 / m_itemWidth * x;
    float y1 = 1 - 2.0 / m_itemHeight * y;
    float x2 = 2.0 / m_itemWidth * frameWidth + x1;
    float y2 = y1 - 2.0 / m_itemHeight * frameHeight;

    mVertices = { Pathfinder::Vec3F(x1, y1, 0.0f), Pathfinder::Vec3F(x2, y1, 0.0f), Pathfinder::Vec3F(x2, y2, 0.0f),
                  Pathfinder::Vec3F(x1, y2, 0.0f) };

    auto encoder = mDevice->create_command_encoder("upload yuv data");

    if (data->linesize[0]) {
        encoder->write_texture(mTexY, {}, data->data[0]);
    }
    if (data->linesize[1]) {
        encoder->write_texture(mTexU, {}, data->data[1]);
    }
    if (data->linesize[2]) {
        encoder->write_texture(mTexV, {}, data->data[2]);
    }

    mQueue->submit_and_wait(encoder);
}

void YuvRenderer::render(std::shared_ptr<Pathfinder::Texture> outputTex) {
    if (!mTextureAllocated) {
        return;
    }
    if (mNeedClear) {
        mNeedClear = false;
        return;
    }

    auto encoder = mDevice->create_command_encoder("render yuv");

    // Update uniform buffers.
    {
        FragUniformBlock uniform = { mPixFmt };

        // We don't need to preserve the data until the upload commands are implemented because
        // these uniform buffers are host-visible/coherent.
        encoder->write_buffer(mUniformBuffer, 0, sizeof(FragUniformBlock), &uniform);
    }

    // Update descriptor set.
    mDescriptorSet->add_or_update(
        {
            Pathfinder::Descriptor::sampled(0, Pathfinder::ShaderStage::Fragment, "y", mTexY, mSampler),
            Pathfinder::Descriptor::sampled(1, Pathfinder::ShaderStage::Fragment, "u", mTexU, mSampler),
            Pathfinder::Descriptor::sampled(2, Pathfinder::ShaderStage::Fragment, "v", mTexV, mSampler),
        });

    encoder->begin_render_pass(mRenderPass, outputTex, Pathfinder::ColorF::black());

    encoder->draw(0, mVertices.size());

    encoder->end_render_pass();

    mQueue->submit_and_wait(encoder);
}

void YuvRenderer::clear() {
    mNeedClear = true;
}
