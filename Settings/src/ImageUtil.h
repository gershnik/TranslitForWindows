// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/err.h>
#include <Common/win32api.h>
#include <Common/ComStream.h>

inline const GUID * getEncoderClsid(const sys_string & format) {
    static std::map<sys_string, GUID> s_map = []() {
        UINT  num = 0;          // number of image encoders
        UINT  size = 0;         // size of the image encoder array in bytes
        auto stat = Gdiplus::GetImageEncodersSize(&num, &size);
        if(stat != Gdiplus::Ok)
            abort();

        std::vector<uint8_t> buf(size);
        auto imageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo *>(buf.data());

        stat = GetImageEncoders(num, size, imageCodecInfo);
        if(stat != Gdiplus::Ok)
            abort();

        std::map<sys_string, GUID> ret;
        for(UINT i = 0; i < num; ++i)
            ret[imageCodecInfo[i].MimeType] = imageCodecInfo[i].Clsid;

        return ret;
    }();

    if (auto it = s_map.find(format); it != s_map.end())
        return &it->second;

    return nullptr;
}

inline std::vector<uint8_t> imageFromIcon(HICON hIcon, const sys_string & format) {

    struct IconInfo : ICONINFO {
        IconInfo(HICON hIcon) {
            if (!GetIconInfo(hIcon, this))
                throwLastError();
        }
        ~IconInfo() {
            ::DeleteObject(hbmColor);
            ::DeleteObject(hbmMask); 
        }
    } iconInfo(hIcon);

    unique_dc dc(CreateCompatibleDC(nullptr));
    if (!dc)
        throwLastError();

    BITMAP bm = {0};
    if (!GetObject(iconInfo.hbmColor, sizeof(bm), &bm))
        throwLastError();

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    int pixelCount = bm.bmWidth * bm.bmHeight;
    std::vector<int32_t> colorPixels(pixelCount);
    GetDIBits(dc.get(), iconInfo.hbmColor, 0, bm.bmHeight, colorPixels.data(), &bmi, DIB_RGB_COLORS);

    auto bitmap = std::unique_ptr<Gdiplus::Bitmap>(new Gdiplus::Bitmap(bm.bmWidth, bm.bmHeight, bm.bmWidth*4, 
                                                                       PixelFormat32bppARGB, 
                                                                       reinterpret_cast<BYTE*>(colorPixels.data())));

    auto * encoderId = getEncoderClsid(format);
    if (!encoderId)
        throwWinError(ERROR_BAD_ARGUMENTS);
    std::vector<uint8_t> ret;
    auto stream = com_attach(new IStreamOnVector(ret));
    auto stat = bitmap->Save(stream.get(), encoderId, nullptr);
    if(stat != Gdiplus::Ok)
        abort();

    return ret;
}


