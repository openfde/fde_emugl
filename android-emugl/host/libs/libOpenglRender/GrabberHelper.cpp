/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Alan Xie    xiehuijun@kylinos.cn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include "GrabberHelper.h"
#include <sys/shm.h>
#include <fcntl.h>
#include <string.h>

static const char vShaderCode[] =
    "attribute vec3 position;\n"
    "attribute vec2 inCoord;\n"
    "varying vec2 outCoord;\n"

    "void main(void) {\n"
    "    gl_Position = vec4(position, 1.0);\n"
    "    outCoord = inCoord;\n"
    "}\n";

static const char fShaderCode[] =
    "precision mediump float;\n"
    "varying lowp vec2 outCoord;\n"
    "uniform sampler2D texture;\n"

    "void main(void) {\n"
    "    vec4 color = texture2D(texture, outCoord);\n"
    "    gl_FragColor = vec4(color.bgr, 1.0);\n"
    "}\n";

GrabberHelper::GrabberHelper(EGLDisplay display, uint32_t width, uint32_t height)
    : m_display(display)
    , m_egl_image(nullptr)
    , m_shader(nullptr)
    , m_width(width)
    , m_height(height)
    , m_vbo(0)
    , m_ebo(0)
    , m_img_tex(0)
    , m_fbo_tex(0)
    , m_fbo(0)
    , tmp_fbo(0)
    , m_img_tex_ready(false)
    , m_sem_for_shm(SEM_FAILED)
    , m_sem_for_grab(SEM_FAILED)
{
    m_data_info.shm_key = KEY_SHM_DATA_INFO;
    m_data_info.shm_id = -1;
    m_data_info.shm_data = (void *) -1;
    memset(&m_screen_image_info, 0, sizeof(m_screen_image_info));

    InitVerticeData();
    InitImageTex();
}

GrabberHelper::~GrabberHelper()
{
    if (m_egl_image) s_egl.eglDestroyImageKHR(m_display, m_egl_image);
    if (m_vbo != 0) s_gles2.glDeleteBuffers(1, &m_vbo);
    if (m_ebo != 0) s_gles2.glDeleteBuffers(1, &m_ebo);
    if (m_img_tex != 0) s_gles2.glDeleteTextures(1, &m_img_tex);
    if (m_fbo_tex != 0) s_gles2.glDeleteTextures(1, &m_fbo_tex);
    if (m_fbo != 0) s_gles2.glDeleteFramebuffers(1, &m_fbo);
    if (tmp_fbo != 0) s_gles2.glDeleteFramebuffers(1, &tmp_fbo);
    if (m_shader) delete m_shader;
    if ((m_data_info.shm_id != -1) && (m_data_info.shm_data != (void *) -1)) {
        shmdt(m_data_info.shm_data);
    }
    if (m_sem_for_shm != SEM_FAILED) {
        sem_close(m_sem_for_shm);
    }
    if (m_sem_for_grab != SEM_FAILED) {
        sem_close(m_sem_for_grab);
    }
}

bool GrabberHelper::GetShm(shm_handle &shm)
{
    if (shm.shm_key == 0) {
        syslog(LOG_WARNING, "[GrabberHelper] Invalid share memory key !");
        return false;
    }
    if ((shm.shm_id == -1) || (shm.shm_data == (void *) -1)) {
        shm.shm_id = shmget(shm.shm_key, 0, 0);// share memory created by app_stream
        if (shm.shm_id == -1) {
            syslog(LOG_ERR, "[GrabberHelper] Error: Can't get share memory (%X)! "
                "Maybe haven't created by app_stream yet!", shm.shm_key);
            return false;
        }
        shm.shm_data = shmat(shm.shm_id, nullptr, 0);
        if (shm.shm_data == (void *) -1) {
            syslog(LOG_ERR, "[GrabberHelper] Error: Can't attach share memory (%X)!", shm.shm_key);
            return false;
        }
        //syslog(LOG_DEBUG, "[GrabberHelper] Get share memory key:0x%X, address: %p", shm.shm_key, shm.shm_data);
    }
    return true;
}

bool GrabberHelper::GetSem()
{
    if (m_sem_for_shm == SEM_FAILED) {
        m_sem_for_shm = sem_open(SEM_NAME_FOR_SHM, O_RDWR);// named semaphore created by app_stream
        if (m_sem_for_shm == SEM_FAILED) {
            syslog(LOG_ERR, "[GrabberHelper] Error: Can't get semaphore for shm! ");
            return false;
        }
    }
    if (m_sem_for_grab == SEM_FAILED) {
        m_sem_for_grab = sem_open(SEM_NAME_FOR_GRAB, O_RDWR);// named semaphore created by app_stream
        if (m_sem_for_grab == SEM_FAILED) {
            syslog(LOG_ERR, "[GrabberHelper] Error: Can't get semaphore for grab! ");
            return false;
        }
    }
    return true;
}

void GrabberHelper::InitVerticeData()
{
    m_shader = new Shader(vShaderCode, fShaderCode);    

    const float vertices[] = {
        // positions          // texture coords
         1.0f,  1.0f, 0.0f,   1.0f, 1.0f,
         1.0f, -1.0f, 0.0f,   1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,   0.0f, 1.0f
    };
    const uint32_t indices[] = {  
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };
    
    s_gles2.glGenBuffers(1, &m_vbo);
    s_gles2.glGenBuffers(1, &m_ebo);

    GLuint pos =  m_shader->getAttribLocation("position");
    GLuint coord =  m_shader->getAttribLocation("inCoord");
    
    s_gles2.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    s_gles2.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    s_gles2.glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    s_gles2.glEnableVertexAttribArray(pos);
    s_gles2.glVertexAttribPointer(coord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    s_gles2.glEnableVertexAttribArray(coord);

    s_gles2.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    s_gles2.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Reset state.
    //s_gles2.glDisableVertexAttribArray(pos);
    //s_gles2.glDisableVertexAttribArray(coord);
    s_gles2.glBindBuffer(GL_ARRAY_BUFFER, 0);
    s_gles2.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GrabberHelper::BindVerticeData()
{
    GLuint pos =  m_shader->getAttribLocation("position");
    GLuint coord =  m_shader->getAttribLocation("inCoord");

    s_gles2.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    s_gles2.glVertexAttribPointer(pos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    s_gles2.glEnableVertexAttribArray(pos);
    s_gles2.glVertexAttribPointer(coord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    s_gles2.glEnableVertexAttribArray(coord);

    s_gles2.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
}

bool GrabberHelper::getGLFormatAndType(PIX_FMT fmt, GLenum *format, GLenum *type)
{
    switch (fmt){
        case PIX_FMT_RGB565:
        case PIX_FMT_RGB555:{
            *format = GL_RGB;
            *type = GL_UNSIGNED_SHORT_5_6_5;
            return true;
        }
        case PIX_FMT_BGR24:
        case PIX_FMT_RGB24:{
            *format = GL_RGB;
            *type = GL_UNSIGNED_BYTE;
            return true;
        }
        case PIX_FMT_BGRA:
        case PIX_FMT_RGBA:
        case PIX_FMT_ABGR:
        case PIX_FMT_ARGB:{
            *format = GL_RGBA;
            *type = GL_UNSIGNED_BYTE;
            return true;
        }
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUV422P:
        case PIX_FMT_YUV444P:
        case PIX_FMT_NV12:
        case PIX_FMT_YUYV422:
        {
            *format = GL_LUMINANCE;
            *type = GL_UNSIGNED_BYTE;
            return true;
        }
        default:
        break;
    }
    syslog(LOG_ERR, "[GrabberHelper] Error: Unsupported image pixel format!");
    return false;
}

bool GrabberHelper::CheckDataInfo(shm_data_info *data_info)
{
    if ((data_info->width == 0) || (data_info->width > MAX_IMAGE_WIDTH)) {
        return false;
    }
    if ((data_info->height == 0) || (data_info->height > MAX_IMAGE_HEIGHT)) {
        return false;
    }
    if (data_info->format == PIX_FMT_UNKNOW) {
        return false;
    }
    return true;
}

bool GrabberHelper::GetScreenImageInfo(ScreenImageInfo &image_info)
{
    if (GetShm(m_data_info)) {
        shm_data_info *data_info = static_cast<shm_data_info *>(m_data_info.shm_data);
        image_info.width = data_info->width;
        image_info.height = data_info->height;
        return true;
    }
    return false;
}

bool GrabberHelper::IsScreenImageChanged()
{
    ScreenImageInfo image_info;
    if (GetScreenImageInfo(image_info)) {
        if ((image_info.width != m_screen_image_info.width) || 
                (image_info.height != m_screen_image_info.height)) { // screen image size changed !
            syslog(LOG_INFO, "[GrabberHelper] Screen image changed !");
            return true;
        }
    }
    return false;
}

void GrabberHelper::InitImageTex()
{
    if (m_img_tex == 0) {
        s_gles2.glGenTextures(1, &m_img_tex);
    }
    s_gles2.glBindTexture(GL_TEXTURE_2D, m_img_tex);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (GetShm(m_data_info)) {
        GetScreenImageInfo(m_screen_image_info);
        shm_data_info *data_info = static_cast<shm_data_info *>(m_data_info.shm_data);
        if (CheckDataInfo(data_info) && getGLFormatAndType(data_info->format, &m_img_tex_gl_format, &m_img_tex_data_type)) {
            s_gles2.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data_info->width, data_info->height, 0,
                m_img_tex_gl_format, m_img_tex_data_type, NULL);
            //s_gles2.glGenerateMipmap(GL_TEXTURE_2D);
            m_img_tex_ready = true;
        }
    }
    s_gles2.glBindTexture(GL_TEXTURE_2D, 0);
}

bool GrabberHelper::UpdateImageTex()
{
    if (!m_img_tex_ready) {
        InitImageTex();
    }

    if (m_img_tex_ready && GetSem()) {
        shm_data_info *data_info = static_cast<shm_data_info *>(m_data_info.shm_data);
        s_gles2.glBindTexture(GL_TEXTURE_2D, m_img_tex);
        sem_wait(m_sem_for_shm);
        if (CheckDataInfo(data_info)) {
            s_gles2.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, data_info->width, data_info->height, 
                                    m_img_tex_gl_format, m_img_tex_data_type, data_info->pixel_data);
            //s_gles2.glGenerateMipmap(GL_TEXTURE_2D);
        }
        else {
            syslog(LOG_WARNING, "[GrabberHelper] Error: Check data info failed!");
        }
        sem_post(m_sem_for_shm);
        sem_post(m_sem_for_grab);
        s_gles2.glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }
    s_gles2.glBindTexture(GL_TEXTURE_2D, 0);
    return false;
}

bool GrabberHelper::BindFbo()
{
    if (m_fbo != 0) {
        s_gles2.glBindTexture(GL_TEXTURE_2D, 0);
        s_gles2.glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        return true;
    }

    s_gles2.glGenFramebuffers(1, &m_fbo);
    s_gles2.glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    s_gles2.glGenTextures(1, &m_fbo_tex);
    s_gles2.glBindTexture(GL_TEXTURE_2D, m_fbo_tex);
    s_gles2.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    s_gles2.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    s_gles2.glBindTexture(GL_TEXTURE_2D, 0);
    s_gles2.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo_tex, 0);

    if (s_gles2.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        s_gles2.glDeleteTextures(1, &m_fbo_tex);
        s_gles2.glDeleteFramebuffers(1, &m_fbo);
        s_gles2.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_fbo = 0;
        syslog(LOG_ERR, "[GrabberHelper] Error: Create framebuffer texture failed!");
        return false;
    }
    return true;
}

void GrabberHelper::UnbindFbo()
{
    s_gles2.glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool GrabberHelper::RenderToFboTex()
{
    if (BindFbo()) {
        BindVerticeData();
        // keep screen aspect
        GLint view_x, view_y, view_w, view_h;
        shm_data_info *data_info = static_cast<shm_data_info *>(m_data_info.shm_data);
        float screenAspect = data_info->width / (float)data_info->height;
        float boAspect = m_width / (float)m_height;
        if (screenAspect >= boAspect) {
            view_w = m_width;
            view_h = view_w / screenAspect;
            view_x = 0;
            view_y = (m_height - view_h) / 2;
        }
        else {
            view_h = m_height;
            view_w = view_h * screenAspect;
            view_x = (m_width - view_w) / 2;
            view_y = 0;
        }
        //syslog(LOG_DEBUG, "[GrabberHelper] width = %d, height = %d, view_w = %d, view_h = %d", 
        //    m_width, m_height, view_w, view_h);
        s_gles2.glViewport(view_x, view_y, view_w, view_h);

        s_gles2.glClearColor(0.f, 0.f, 0.f, 1.f);
        s_gles2.glClear(GL_COLOR_BUFFER_BIT);

        m_shader->use();
        s_gles2.glActiveTexture(GL_TEXTURE0);
        s_gles2.glBindTexture(GL_TEXTURE_2D, m_img_tex);

        s_gles2.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        s_gles2.glFinish();

        UnbindFbo();
        s_gles2.glBindVertexArray(0);
        return true;
    }
    return false;
}
/*
inline int64_t hrt_time_micro() 
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * (uint64_t) 1000000 + (uint64_t) (ts.tv_nsec / 1000);
}
*/
bool GrabberHelper::UpdateAndReadPixel(int x, int y, int width, int height, GLenum p_format, GLenum p_type, void* pixels)
{
    if (x != 0 || y != 0 || width != m_width || height != m_height) {
        syslog(LOG_ERR, "[GrabberHelper] Error: Rect not match!");
        return false;
    }
/*
    float fps = 0;
    static int64_t pre_time = 0, now_time = 0;
    if (pre_time == 0) {
        pre_time = hrt_time_micro();
    }
    now_time = hrt_time_micro();
    if (now_time - pre_time > 1000) {
        fps = 1000000 / (float)(now_time - pre_time);
    }
    pre_time = now_time;
*/
    if (UpdateImageTex() && RenderToFboTex()) {
        bool tmp = ReadFboTexPixel(x, y, width, height, p_format, p_type, pixels);
        //int64_t tmp_time = hrt_time_micro();
        //syslog(LOG_DEBUG, "[GrabberHelper][%s] fps = %02f, coast %d us per frame.", 
        //    __func__, fps, tmp_time - now_time);
        return tmp;
    }
    return false;
}

bool GrabberHelper::ReadFboTexPixel(int x, int y, int width, int height, GLenum p_format, GLenum p_type, void* pixels)
{
    if (BindFbo()) {
        GLint prevAlignment = 0;
        s_gles2.glGetIntegerv(GL_PACK_ALIGNMENT, &prevAlignment);
        s_gles2.glPixelStorei(GL_PACK_ALIGNMENT, 1);
        s_gles2.glReadPixels(x, y, width, height, p_format, p_type, pixels);
        s_gles2.glPixelStorei(GL_PACK_ALIGNMENT, prevAlignment);
        UnbindFbo();
        return true;
    }
    return false;
}

bool GrabberHelper::updataEglImage()
{
    //syslog(LOG_DEBUG, "[GrabberHelper][%s]", __func__);
    if (UpdateImageTex() && RenderToFboTex()) {
        if (!m_egl_image) {
            m_egl_image = s_egl.eglCreateImageKHR(
                m_display, s_egl.eglGetCurrentContext(), EGL_GL_TEXTURE_2D_KHR,
                (EGLClientBuffer)(void*)(uintptr_t)(m_fbo_tex), NULL);
        }
        if (m_egl_image) {
            return true;
        }
        else {
            syslog(LOG_ERR, "[GrabberHelper][%s] egl image is invalid!", __func__);
        }
    }
    else {
        syslog(LOG_ERR, "[GrabberHelper][%s] update image or render to fbo failed!", __func__);
    }

    return false;
}

void GrabberHelper::bindEglImage()
{
    //syslog(LOG_DEBUG, "[GrabberHelper][%s]", __func__);
    if (m_egl_image) {
        s_gles2.glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_egl_image);
    }
}
