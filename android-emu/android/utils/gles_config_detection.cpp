/*
 * Copyright (c) KylinSoft Co., Ltd. 2016-2024.All rights reserved.
 *
 * Authors:
 *  Clom Huang   huangcailong@kylinos.cn
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

#include "gles_config_detection.h"

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <string>
#include <pwd.h>
#include "syslog.h"

#define BUF_SIZE 4096

static std::string getCurrentUserName() {
    std::string userName;

    char user[1024] = {0};
    const char* name = nullptr;
    if ((name = getenv("USER")) != nullptr) {
        snprintf(user, sizeof(user), "%s", name);
    } else if ((name = getenv("USERNAME")) != nullptr) {
        snprintf(user, sizeof(user), "%s", name);
    }
    userName = user;

    if (getlogin()) {
        sprintf(user, "%s", getlogin());
    } else {
        struct passwd  pwd;
        struct passwd* result = 0;
        char buf[1024];

        memset(&buf, 0, sizeof(buf));
        uint32_t uid = getuid();
        (void)getpwuid_r(uid, &pwd, buf, 1024, &result);
        if (!result) {
            syslog(LOG_DEBUG, "getpwnam_r error,uid = %d",uid);
            fprintf(stderr, "getpwnam_r error,uid = %d\n",uid);
        } 
        if (!(pwd.pw_name)) {
            fprintf(stderr, "Failed to get user name from uid.\n");
            syslog(LOG_ERR, "utils: Failed to get user name from uid.");
            return userName;
        }

        char* _user = pwd.pw_name;
        memset(user, sizeof(user), 0);
        sprintf(user, "%s", _user);

        struct passwd  pwd1;
        struct passwd* result1 = 0;
        char buf1[1024];

        memset(&buf1, 0, sizeof(buf1));
        
        (void)getpwnam_r(_user, &pwd1, buf1, 1024, &result1);
        if (!result1) {
            syslog(LOG_DEBUG, "getpwnam_r error,userName = %s",_user);
            fprintf(stderr, "getpwnam_r error,userName = %s\n",_user);
        }
        if (pwd1.pw_uid != getuid()) {
            fprintf(stderr, "User name doesn't match uid.\n");
            syslog(LOG_ERR, "utils: User name doesn't match uid.");
            return userName;
        }
    }

    userName = std::string(user);
    return userName;
}

static std::string convertUserNameToPath(const std::string& userName)
{
    char buffer[BUF_SIZE] = {0};
    std::string path = userName;
    unsigned int i = 0;
    const char* str = nullptr;

    str = userName.c_str();
    if (str && strstr(str, "\\")) {
        snprintf(buffer, sizeof(buffer), "%s", str);
        for (i = 0; i < sizeof(buffer); ++i) {
            if ('\0' == buffer[i]) {
                break;
            }

            if ('\\' == buffer[i]) {
                buffer[i] = '_';
            }
        }

        path = buffer;
    }

    return path;
}

GLESVersionType GLESConfigDetection::mGLESVersionType = GLESVersion_unknown;
FeatureStatus GLESConfigDetection::mEGLStatus = Status_unknown;
//bool GLESConfigDetection::mEgl2Egl = false;
bool GLESConfigDetection::mGLESConfigExist = false;
bool GLESConfigDetection::mEnableASTC = false;
FeatureStatus GLESConfigDetection::mNTDCStatus = Status_unknown;
static bool readConfigCompleted = false;

void GLESConfigDetection::readRenderGLESConfig() {
    if (readConfigCompleted) {
        return;
    }
    std::string glesSettingsPath;
    const char* home = getenv("HOME");

    if (home) {
        glesSettingsPath = std::string(home) + "/.config/kmre/render_gles";
    } else {
        glesSettingsPath = "/home/" + convertUserNameToPath(getCurrentUserName()) + "/.config/kmre/render_gles";
    }
    
    FILE* fp = NULL;
    char line[256] = {0};

    fp = fopen(glesSettingsPath.c_str(), "r");
    if (!fp)
    {
        fprintf(stderr, "gles version detection file may no found!\n");
        readConfigCompleted = true;
        mGLESConfigExist = false;
        mEnableASTC = false;        
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strcasestr(line, "version=2.0")) {
            mGLESVersionType = GLESVersion_2_0;            
        } else if (strcasestr(line, "version=3.0")) {
            mGLESVersionType = GLESVersion_3_0;            
        } else if (strcasestr(line, "version=3.1")) {
            mGLESVersionType = GLESVersion_3_1;              
        } else if (strcasestr(line, "egl2egl=true")) {
            mEGLStatus = Status_enable;
        } else if (strcasestr(line, "egl2egl=false")) {
            mEGLStatus = Status_disable;
        } else if (strcasestr(line, "astc=true")) {
            syslog(LOG_DEBUG,"read config astc is true");
            mEnableASTC = true;
        } else if (strcasestr(line, "ntdc=true")) {
            syslog(LOG_DEBUG,"read config NativeTextureDecompression is true");
            mNTDCStatus = Status_enable;
        } else if (strcasestr(line, "ntdc=false")) {
            syslog(LOG_DEBUG,"read config NativeTextureDecompression is true");
            mNTDCStatus = Status_disable;
        }
    }
    fclose(fp);
    readConfigCompleted = true;
    mGLESConfigExist = true;
    return;
}

GLESVersionType GLESConfigDetection::getGLESVersion() {
    if (mGLESVersionType != GLESVersion_unknown) {
        return mGLESVersionType;
    } else {
        readRenderGLESConfig();
        return mGLESVersionType;
    }
}

FeatureStatus GLESConfigDetection::getEGLStatus() {
    if (mEGLStatus != Status_unknown) {
        return mEGLStatus;
    } else {
        readRenderGLESConfig();
        return mEGLStatus;
    }
}

bool GLESConfigDetection::isGLESConfigExist() {
    if (mGLESConfigExist) {
        return mGLESConfigExist;
    } else {
        readRenderGLESConfig();
        return mGLESConfigExist;
    }
}

bool GLESConfigDetection::isEnableASTC() {
    if (mEnableASTC) {
        return mEnableASTC;
    } else {
        readRenderGLESConfig();
        return mEnableASTC;
    }
}

FeatureStatus GLESConfigDetection::getNTDCStatus() {
    if (mNTDCStatus != Status_unknown) {
        return mNTDCStatus;
    } else {
        readRenderGLESConfig();
        return mNTDCStatus;
    }
}