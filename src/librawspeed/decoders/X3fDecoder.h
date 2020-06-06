/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2020 Yanan Wen

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#pragma once

#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "io/ByteStream.h"                // for ByteStream
#include "tiff/TiffIFD.h"                 // for TiffIFD (ptr only), TiffRo...
#include <cstdint>                        // for uint32_t
#include <utility>                        // for move

namespace rawspeed {

class CameraMetaData;
class Buffer;

class X3fDirectory {
public:
    X3fDirectory() : id(std::string()) {}
    explicit X3fDirectory(ByteStream* bytes);
    uint32_t offset{0};
    uint32_t length{0};
    std::string id;
    std::string sectionID;
};

class X3fDecoder final : public RawDecoder
{
public:
    static bool isX3f(const Buffer* input);
    static bool isAppropriateDecoder(const Buffer* file);
    explicit X3fDecoder(const Buffer* file) : RawDecoder(file) {}
    RawImage decodeRawInternal() override;
    void checkSupportInternal(const CameraMetaData* meta) override;
    void decodeMetaDataInternal(const CameraMetaData* meta) override;

protected:
    int getDecoderVersion() const override { return 0; }


};

} // namespace rawspeed
