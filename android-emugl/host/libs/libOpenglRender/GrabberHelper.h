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
 
#pragma once
#include "DispatchTables.h"
#include "OpenGLESDispatch/EGLDispatch.h"
#include <syslog.h>
#include <semaphore.h>
#include <string>
#include <shared_mutex>

#define MAX_IMAGE_WIDTH         20000
#define MAX_IMAGE_HEIGHT        20000
#define KEY_SHM_DATA_INFO       0x68788898
#define SEM_NAME_FOR_SHM        "sem_shm"
#define SEM_NAME_FOR_GRAB       "sem_grab"

enum PIX_FMT{
    PIX_FMT_UNKNOW = 0,
    PIX_FMT_PAL8,
    PIX_FMT_RGB565,
    PIX_FMT_RGB555,
    PIX_FMT_BGR24,
    PIX_FMT_RGB24,
    PIX_FMT_BGRA,
    PIX_FMT_RGBA,
    PIX_FMT_ABGR,
    PIX_FMT_ARGB,
    PIX_FMT_YUV420P,
    PIX_FMT_YUV422P,
    PIX_FMT_YUV444P,
    PIX_FMT_NV12,
    PIX_FMT_YUYV422,
};

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t size;
    PIX_FMT format;
    void *pixel_data;
}shm_data_info;

typedef struct {
    uint32_t width;
    uint32_t height;
}ScreenImageInfo;

typedef struct {
    uint32_t shm_key;
    int shm_id;
    void *shm_data;
}shm_handle;

class GrabberHelper
{
public:
    GrabberHelper(EGLDisplay display, uint32_t width, uint32_t height);
    virtual ~GrabberHelper();  

    class Shader
    {
    public:
        Shader(const char* vShaderCode, const char* fShaderCode):m_program_id(0)
        {
            uint32_t vertex, fragment;
            // vertex shader
            vertex = s_gles2.glCreateShader(GL_VERTEX_SHADER);
            s_gles2.glShaderSource(vertex, 1, &vShaderCode, NULL);
            s_gles2.glCompileShader(vertex);
            checkCompileErrors(vertex, "VERTEX");
            // fragment Shader
            fragment = s_gles2.glCreateShader(GL_FRAGMENT_SHADER);
            s_gles2.glShaderSource(fragment, 1, &fShaderCode, NULL);
            s_gles2.glCompileShader(fragment);
            checkCompileErrors(fragment, "FRAGMENT");
            // shader Program
            m_program_id = s_gles2.glCreateProgram();
            s_gles2.glAttachShader(m_program_id, vertex);
            s_gles2.glAttachShader(m_program_id, fragment);
            s_gles2.glLinkProgram(m_program_id);
            checkCompileErrors(m_program_id, "PROGRAM");
            // delete the shaders
            s_gles2.glDeleteShader(vertex);
            s_gles2.glDeleteShader(fragment);
        }
        ~Shader()
        {
            if (m_program_id != 0) {
                s_gles2.glDeleteProgram(m_program_id);
                m_program_id = 0;
            }
        }

        void use() const
        { 
            s_gles2.glUseProgram(m_program_id); 
        }
        void setBool(const char *name, bool value) const
        {         
            s_gles2.glUniform1i(getUniformLocation(name), (int)value); 
        }
        void setInt(const char *name, int value) const
        { 
            s_gles2.glUniform1i(getUniformLocation(name), value); 
        }
        void setFloat(const char *name, float value) const
        { 
            s_gles2.glUniform1f(getUniformLocation(name), value); 
        }
        GLuint getAttribLocation(const char *name) const
        {
            return s_gles2.glGetAttribLocation(m_program_id, name);
        }
        GLuint getUniformLocation(const char *name) const
        {
            return s_gles2.glGetUniformLocation(m_program_id, name);
        }
        
    private:
        uint32_t m_program_id;

        void checkCompileErrors(uint32_t shader, std::string type)
        {
            int success;
            char infoLog[1024];
            if (type != "PROGRAM") {
                s_gles2.glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
                if (!success) {
                    s_gles2.glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                    syslog(LOG_ERR, "[Shader] ERROR: SHADER_COMPILATION_ERROR, type: %s,infoLog: %s", type.c_str(), infoLog);
                }
            }  else {
                s_gles2.glGetProgramiv(shader, GL_LINK_STATUS, &success);
                if (!success) {
                    s_gles2.glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                    syslog(LOG_ERR, "[Shader] ERROR: PROGRAM_LINKING_ERROR, type: %s,infoLog: %s", type.c_str(), infoLog);
                }
            }
        }
    };

private:
    void InitVerticeData();
    void BindVerticeData();
    bool getGLFormatAndType(PIX_FMT fmt, GLenum *format, GLenum *type);
    void InitImageTex();
    bool BindFbo();
    void UnbindFbo();
    bool RenderToFboTex();
    bool UpdateImageTex();
    bool ReadFboTexPixel(int x, int y, int width, int height, GLenum p_format, GLenum p_type, void* pixels);
    bool GetShm(shm_handle &shm);
    bool CheckDataInfo(shm_data_info *data_info);
    bool GetSem();
    bool GetScreenImageInfo(ScreenImageInfo &image_info);

private:
    EGLDisplay m_display;
    EGLImageKHR m_egl_image;
    Shader *m_shader;

    ScreenImageInfo m_screen_image_info;
    uint32_t m_width, m_height;
    uint32_t m_vbo, m_ebo;
    uint32_t m_img_tex, m_fbo_tex;
    uint32_t m_fbo, tmp_fbo;

    bool m_img_tex_ready;
    GLenum m_img_tex_gl_format, m_img_tex_data_type;
    shm_handle m_data_info;
    sem_t *m_sem_for_shm, *m_sem_for_grab;

public:
    bool UpdateAndReadPixel(int x, int y, int width, int height, GLenum p_format, GLenum p_type, void* pixels);
    uint32_t getWidth(){return m_width;}
    uint32_t getHeight(){return m_height;}
    bool IsScreenImageChanged();
    bool updataEglImage();
    void bindEglImage();
};



